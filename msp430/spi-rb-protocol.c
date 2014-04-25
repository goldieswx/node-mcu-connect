/*
    node-mcu-connect . node.js UDP Interface for embedded devices.
    Copyright (C) 2013-4 David Jakubowski

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/


#include "msp430g2553.h"
#include <legacymsp430.h>

void inline transfer(char s) {
    while (!(IFG2 & UCB0TXIFG));
    UCB0TXBUF = s;
}

void inline signalMaster () {
   IFG2 &= ~UCA0TXIFG;
   UCA0TXBUF = 0xFF;
}

unsigned char id = 0x02;

unsigned char bfr[66];
unsigned char lastresp[20];
unsigned char lastrespLen;

unsigned  char * p, * boundary;

unsigned int state;

#define MI_HEADER_SIZE 4
#define MI_PREAMBLE 0b10101100
#define MI_CMD 8714
#define MI_ANSWER 9934

#define SNCC_CMD     12453  // Mi issues an explicit SNCC 
#define MI_CHECK     12554  // (A) Slave(s) brought up MISO, who is(are) it(they)
#define MI_UPBFR     22342
#define MI_UPBFRAM   21233
#define MI_AM        3409
  

void execCmd(int i) {

     unsigned char bfrStart[] = {0,4,20,32,64};

     unsigned char * r = &lastresp[lastrespLen];
     unsigned char * q = &bfr[bfrStart[i]];
     
         *r++ = 0xAB;
         *r++ = 0xCD;
         *r++ = 0xEC;
         *r++ = 0x49;
         lastrespLen+=4;
     
       P1OUT  |=  *q++; //BIT0 | BIT3;
       P1OUT  &=  *q++;
     
        TA0R = 0;
        TA0CCR0 = 32767;// 32767;                          // Count to this, then interrupt
 

     return;
}


void zeroMem(unsigned char * p) {
     //*p = 0x00;
     while (p != boundary) { *p++ = 0x00; }
     *p++ = 0x00; 
}

void notifyCmd() {

     unsigned int * p = (unsigned int*) &bfr[2];
     unsigned int mask;

     if (1) { // if we have something to tell insert our id in the mask
         mask = 1 << id;
         *p = mask; 
     }
}

void storeResponse(i) {

    zeroMem(bfr);
    bfr[16] = 0xab;


}


void checkDAC(int * inbfr) {

    char chan;

    for(chan=0;chan<7;chan++) {
      
      // bring cs high
      P2OUT |= p2Mask;

      char d[3];
      d[0] = 0b00000001; // start bit    
      d[1] = chan << 4;
      d[2] = 0;

      // bring cs low (active)
      P2OUT &= ~p2Mask;
      
      // start spi transfer
      // Ob00000001  // start bit
      // 0b0ddd00xx chan
      // 0bxxxxxxxx
      int i;
      while (i=0;i<3;i++) {
          transfer(d[i]);
          d[i] =  UCB0RXBUF;
      }

      d[1] &= 0b00000011;
      *inbfr++ = (int*)(&d[1]);
   }
   // cs left low (power saving)  

} 


inline void processBuffer() {

    p = bfr;
    int bfrlen = boundary - p; 
    int nextLength;
    int i;

    // check MI header 
    if (state == 0) {
      if (bfrlen == MI_HEADER_SIZE) {
          int currCmd = *((int*)(&bfr[2]));

          if (bfr[0] == MI_PREAMBLE) {
             
              if (currCmd == MI_CMD) {
                 nextLength = bfr[1];
                 zeroMem(p);
                 boundary = bfr + nextLength; 
                 state = currCmd;
                 // boundary set (to presumably 64 bytes)
              }

              if (currCmd == SNCC_CMD) {
                 zeroMem(p);
                 //boundary = boundary unchanged 
                 state = currCmd;
              }


          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = 0;
          }
      }
      return;
    }

    // we have received a command buffer, process it
    if (state == MI_CMD) {
      lastrespLen = 0;     
      if (bfr[0] != MI_PREAMBLE) { 
        zeroMem(p);
        return; 
      }
      
      char j = 0xff;
      // exec cmds in bfr
      for (i=1;i<=3;i++) {
         if (bfr[i] == id) {
             execCmd(i); // sets lastresp & lastresplen.
             j = i;
         }
      }   

      // store responses in bfr (last slot if we had several execCmds)
      if (j<=3) {
        zeroMem(p);
        unsigned char respChecksum = 0;
        p = &bfr[4+(j-1)*16];
        for (i=0;i<lastrespLen;i++) {
           *p = lastresp[i];
           respChecksum += *p++;
        }
        *p = ~respChecksum;
        p=bfr;
        state = MI_ANSWER;
        return;
     }
      
     p = bfr; // boundary unchanged


     state = MI_ANSWER;
     zeroMem(p);
     return;
    }

    // we shall acknowlege that our cmd is processed and return result.
    if (state == MI_ANSWER) {

       state = 0;
       p = bfr;
       boundary = &bfr[MI_HEADER_SIZE];
       zeroMem(p);
       return;
    }

    // someone (maybe us) signalled master in SNCC. do we have something to say ?
    if (state == SNCC_CMD)  {
        if (bfrlen == MI_HEADER_SIZE) {
          int currCmd = *((int*)(&bfr[2]));

          if (bfr[0] == MI_PREAMBLE) {
              if (currCmd == MI_CHECK) {
                 zeroMem(p);
                 notifyCmd();           
                 state = currCmd;
              }
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
          }
      }
      return;
    }

    // someone (maybe us) signalled master in SNCC. do we have something to say ?
    if (state == MI_CHECK)  {
       // MI_UPID exchange, we get where to send the response.
      
        if (bfrlen == MI_HEADER_SIZE) {
          if (bfr[0] == MI_PREAMBLE) {
             for (i=1; i<4; i++) {
                 if (id == bfr[i]) {
                    storeResponse (i);
                    boundary = &(bfr[64]);
                 }
             }
             state = MI_UPBFR;
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
          }
      }
      return;
    }


    // buffer was just being sent, update state to SNCC
    if (state == MI_UPBFR)  {
        if (bfrlen >= MI_HEADER_SIZE) {
          int currCmd = *((int*)(&bfr[2]));

          if (bfr[0] == MI_PREAMBLE) {
              if (currCmd == MI_UPBFR) {
                 zeroMem(p);
                 state = 0;
                 boundary = &(bfr[MI_HEADER_SIZE]);
              }
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
          }
      }
      return;
    }

    boundary = &(bfr[MI_HEADER_SIZE]);
    return;

}


int  main(void) {
  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer

  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
  P1REN &= 0; 
  P1DIR |= BIT0 | BIT3 | BIT1;
  
  P1OUT &= 0;
  
  state = 0;

  unsigned long int cycles;
  // find 20 (100us) low cycles (so there's good chance transmission is finished).
  while (1) {
    cycles = 0;
    while (P1IN & BIT4); // wait for low level clock
    while ((!(P1IN & BIT4)) && (cycles < 20)) { // wait for 160 cycles of low level clock
       cycles++;
    }
    if (cycles >= 2) break;
  }
//    P1OUT &= ~BIT3;

  // prep SLAVE SPI (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
  // prev MASTER SPI (bit 5 CLK,6 MISO, 7 MOSI)
  P1SEL =  BIT1 + BIT2 + BIT4 + BIT5 + BIT6 + BIT7 ; 
  P1SEL2 =  BIT1 + BIT2 + BIT4  + BIT5 + BIT6 + BIT7 ;
  
  P2SEL = BIT0;
  P2OUT &= 0;
  
  
  UCA0CTL1 = UCSWRST;                       // **Put state machine in reset**
  UCA0CTL0 |= UCCKPH + UCMSB  + UCSYNC;     // 3-pin, 8-bit SPI slave
  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

  while(IFG2 & UCA0RXIFG);        // wait ifg2 flag on rx  (no idea what it does)
  IE2 |= UCA0RXIE;                          // Enable USCI0 RX interrupt
  UCA0TXBUF = 0x00;         // We do not want to ouput anything on the line
 
 
  UCB0CTL1 = UCSWRST + UCSSEL2; // or UCSSEL3  ?
  UCB0CTL0 |= UCCKPH + UCMSB + UCSYNC + UCMST;
  UCB0CTL1 &= ~UCSWRST; 
  UCB0TXBUF = 0x00; 


  BCSCTL3 |= LFXT1S_2;                      // Set clock source to VLO
  TA0R = 0;
  TA0CCR0 = 0;// 32767;                          // Count to this, then interrupt;  0 to stop counting
  TA0CTL = TASSEL_1 | MC_1;                 // Clock source ACLK
  TA0CCTL1 = CCIE ;                          // Timer A interrupt enable


  p = &bfr[0];
  boundary = &bfr[MI_HEADER_SIZE]; // waits for MI header by default
  zeroMem(p);

  while(1) {
    __bis_SR_register(LPM3_bits + GIE);   // Enter LPM3, enable interrupts // we need ACLK for timeout.
    processBuffer(); // after boundary bytes received, process buffer.

  };      
 
  return 0;
}

interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

   //WDTCTL = WDTHOLD;  // Write to WD without password to reboot.

   TA0CCTL1 = ~CCIFG;  
   TA0R = 0;
   
   P1OUT ^= BIT0 | BIT3;
     
   signalMaster();

   TA0CCR0 = 0;
   TA0CCTL1 = CCIE ;   
   return;
} 


interrupt(PORT2_VECTOR) p2_isr(void) {

   //WDTCTL = WDTHOLD;  // Write to WD without password to reboot.
   char[] myBuf = {0x8,0x9,0xa};
   sendSPI(myBuf, 3, BIT0);
   return;
} 



interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

      //register unsigned short x asm("r10");

      (*p++) = UCA0RXBUF;
      UCA0TXBUF = (*p);
      //TA0CCTL1 = CCIE;
      if (p == boundary) {
        LPM3_EXIT;
      }
      return;
 }
