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


unsigned char id = 0x02;

unsigned char bfr[66];
unsigned char lastresp[20];
unsigned char lastrespLen;

unsigned  char * p, * boundary;

unsigned int state;
unsigned int action;
unsigned int silentAction;

#define MI_HEADER_SIZE 4
#define MI_PREAMBLE 0b10101100
#define MI_CMD 8714
#define MI_ANSWER 9934

#define SNCC_CMD     12453  // Mi issues an explicit SNCC 
#define MI_CHECK     12554  // (A) Slave(s) brought up MISO, who is(are) it(they)
#define MI_UPBFR     22342
#define MI_UPBFRAM   21233
#define MI_AM        3409
  
#define PROCESS_BUFFER 0X01
#define ADC_CHECK      0x02
#define SIGNAL_MASTER  0X04

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5


#define CS_NOTIFY_MASTER  BIT3   // External Interrupt 
#define CS_INCOMING_PACKET  BIT4   // Master enable the line before sending



char transfer(char);
void inline signalMaster ();
void execCmd(int);
void zeroMem(unsigned char *);
void copyMem(unsigned char *, unsigned char *, unsigned int);
void notifyCmd();
void storeResponse();
void checkADC(); 
void processBuffer();

void initMCOM();
void initADCE();
void initTimers();


int  main(void) {

  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
  BCSCTL3 |= LFXT1S_2;                      // Set clock source to VLO (low power osc for timer)

  P1REN &= 0; 
  P1OUT &= 0;
  P2REN &= 0;
  P2OUT &= 0;

  P2SEL2 &= 0;
  P2SEL &= 0;
  
  P2DIR |= BIT6 + BIT7;  // debug;


  state = 0;
  action = 0;
  silentAction = 0;

  initMCOM();
  initADCE();
  initTimers();

  p = &bfr[0];
  boundary = &bfr[MI_HEADER_SIZE]; // waits for MI header by default
  zeroMem(p);

  while(1) {
   
     __enable_interrupt();
    if (action == 0) {
      __bis_SR_register(LPM3_bits + GIE);   // Enter LPM3, enable interrupts // we need ACLK for timeout.
    }
    __disable_interrupt(); // process pending interrupts
     
        
  if (action & PROCESS_BUFFER) {
        processBuffer(); 
        continue;
    }
    if (action & ADC_CHECK) { 
        checkADC();
        continue;
    }
    if (action & SIGNAL_MASTER) {
        signalMaster();
        continue;
    }
  };      
 
  return 0;
}


interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {


  TA0CTL = TACLR;  // stop & clear timer
  TA0CCTL1 &= 0;   // also disable timer interrupt & clear flag

  return;
} 

interrupt(PORT2_VECTOR) p2_isr(void) {

  //__enable_interrupt();
  if (P2IFG & CS_NOTIFY_MASTER) {
    P2IE &= ~CS_NOTIFY_MASTER;
    action |= ADC_CHECK;
    __bic_SR_register_on_exit(LPM3_bits /*+ GIE*/); // exit LPM     


  }

  return;
  
} 

interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

  //if (IFG2 & UCA0RXIFG) {
    (*p++) = UCA0RXBUF;
    UCA0TXBUF = (*p);
    if (p == boundary) {
      action |= PROCESS_BUFFER;
      __bic_SR_register_on_exit(LPM3_bits /*+ GIE*/); // exit LPM      
    }
  //}

  return;
}


/**
 *  ADCE Related 
 */

void initADCE() {
    
  P2DIR &= ~CS_NOTIFY_MASTER ;
  P2DIR |= CS_INCOMING_PACKET;
  P2IE |=  CS_NOTIFY_MASTER ; 
  P2IES &= ~CS_NOTIFY_MASTER ;    

  P2REN |=  CS_INCOMING_PACKET;

  // UARTB 
  // Comm channel with extentions
  
  P1DIR |= BIT5 | BIT7;
  P1DIR &= ~BIT6;

  //power the extension
  P1DIR |= BIT3;
  P1OUT |= BIT3;

}

void delayCyclesProcessBuffer(unsigned int nTimes) {

    // delay cycles but not too much while allowing the  buffer processing.
    // interrupts should be enabled otherwise this a few meaning.
    while (nTimes--) {
        __delay_cycles(250);           
        if (action & PROCESS_BUFFER) {
            processBuffer();
            __enable_interrupt(); 
        }
    }
    return;

}


void checkADC() {
    

    P2IE &= ~CS_NOTIFY_MASTER;
    P2IFG |= CS_NOTIFY_MASTER;    // Just preacaution, we will soon enable interrupts, make sure we don't allow reenty.
    __enable_interrupt();         // Our process is low priority, only listenning to master spi is the priority.
    
    action &= ~ADC_CHECK;         // Clear current action flag.
    
 //   P2OUT ^= BIT7;  // debug        // ADC Extension (ADCE) is a module ocnnected thru USCI-B and two GPIO pins
    P2OUT |= CS_INCOMING_PACKET;    // Warn ADCE that we are about to start an spi transfer.
    
    delayCyclesProcessBuffer(20); // Give some time to ADCE to react

    unsigned char c[16];
    unsigned char * p = c;
    
    *p = transfer(0);               // Get Packet length from ADCE
    
    int len = *p++ & 0b00001111;
    int len2 = len;

    delayCyclesProcessBuffer(8);

    while (len--) {
        *p++ = transfer(0);
        delayCyclesProcessBuffer(8);
    }


    lastresp[16] = c[0];            // Temporarily set the response somewhere (FIX)
    lastresp[17] = c[1];
    lastresp[18] = c[2];
    lastresp[19] = 0x07;

    action |= SIGNAL_MASTER;        // Inform master we have some data to transmit.

    __disable_interrupt();


}

/**
 * MCOM Related
 */


void initMCOM() {
 
  // UART A (main comm chanell with MASTER)
  //     (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 
 // P1DIR |= BIT1;
  P1SEL |=  BIT1 | BIT2 | BIT4;
  P1SEL2 |=  BIT1 | BIT2 | BIT4;
 
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
    
  UCA0CTL1 = UCSWRST;                       // **Put state machine in reset**
  UCA0CTL0 |= UCCKPH + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

  while(IFG2 & UCA0RXIFG);                  // Wait ifg2 flag on rx  (no idea what it does)
  IE2 |= UCA0RXIE;                          // Enable USCI0 RX interrupt
  UCA0TXBUF = 0x00;                         // We do not want to ouput anything on the line

}


void inline signalMaster () {

   action &= ~SIGNAL_MASTER;

   if (state == SNCC_CMD) {
     IFG2 &= ~UCA0TXIFG;
     UCA0TXBUF = 0xFF;
   } else {
     silentAction |= SIGNAL_MASTER;
   }

}


void execCmd(int i) {

     unsigned char bfrStart[] = {0,16,32,48};

     unsigned char * r = &lastresp[lastrespLen];
     unsigned char * q = &bfr[bfrStart[i]];

      P2OUT  &=  *q++; 
      P2OUT  |=  *q++;

     
     *r++ = 0xAB;
     *r++ = 0xCD;
     *r++ = 0xEC;
     *r++ = 0x49;
     lastrespLen+=4;
     
     
     return;
}


void notifyCmd() {

     unsigned int * p = (unsigned int*) &bfr[2];
     unsigned int mask;

     if (1) { // if we have something to tell insert our id in the mask
         mask = 1 << id;
         *p = mask; 
     }
}

void storeResponse() {

    zeroMem(bfr);
    bfr[16] = 0xab;
    int i;
    for (i=0;i<20;i++) {
      bfr[i+1] = lastresp[i];
    }
}

void onMasterSignalled () {


     // master has been signalled a bit before.
     // however we don't want the ext module to flood us with a new request
     // therefore we have waited to be in SNCC mode to service this.

     P2IFG &= ~CS_NOTIFY_MASTER;    // clear P2 IFG    
     P2IE |= CS_NOTIFY_MASTER;      // enable P2 Interrupt */    

     P2OUT &= ~CS_INCOMING_PACKET;

}

void processBuffer() {

    __disable_interrupt();      

    action &= ~PROCESS_BUFFER;

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
                 bfr[3] = 0x80;
                 // boundary set (to presumably 64 bytes)
              }

              if (currCmd == SNCC_CMD) {
                 zeroMem(p);
                 //boundary = boundary unchanged 
                 state = currCmd;
                 bfr[3] = 0x81;

                 if (P2IFG & CS_NOTIFY_MASTER) {
                     onMasterSignalled();
                 }
                 if (silentAction & SIGNAL_MASTER) {
                     silentAction &= ~SIGNAL_MASTER;
                     action |= SIGNAL_MASTER;
                     bfr[3] = 0x82;
                 }
              }
          } else {
             // manage error
             zeroMem(p);
             bfr[3] = 0x83;

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
        unsigned int respChecksum = 0;
        unsigned int reqChecksum = 0;
        p = &bfr[j*16];
        for (i=0;i<16;i++) {
           if (i<14) { // do not count checksum
            reqChecksum += *p;
           }
           if (i<lastrespLen) {
             *p = lastresp[i];
           } else {
             *p = 0x00;
           }
           respChecksum += *p++;
        }
        
        // put response checksum at byte 12
        p = &bfr[(j)*16];
        *((unsigned int*)(p+12)) = ~respChecksum;
        // put request checksum at byte 14
        *((unsigned int*)(p+14)) = ~reqChecksum;
        /// 
        p=bfr;
        state = MI_ANSWER;
        return;
     }
      
     p = bfr; // boundary unchanged


     state = MI_ANSWER;
     zeroMem(p);
     bfr[3] = 0x86;

     return;
    }

    // we shall acknowlege that our cmd is processed and return result.
    if (state == MI_ANSWER) {

       state = 0;
       p = bfr;
       boundary = &bfr[MI_HEADER_SIZE];
       zeroMem(p);
       bfr[3] = 0x87;

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
                 bfr[3] = 0x88;

              }
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
             bfr[3] = 0x89;

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
                    bfr[3] = 0x8A;

                 }
             }
             state = MI_UPBFR;
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
             bfr[3] = 0x8B;

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
                 bfr[3] = 0x8C;


              }
          } else {
             // manage error
             zeroMem(p);
             boundary = &(bfr[MI_HEADER_SIZE]);
             state = SNCC_CMD;
             bfr[3] = 0x8D;

          }
      }
      return;
    }

   bfr[3] = 0x8E;
    boundary = &(bfr[MI_HEADER_SIZE]);
    return;

}


/**
 *  Timers
 */

void initTimers() {

  // Timer A0
    
  TA0R = 0;
  TA0CCR0 = 0;// 32767;                  // Count to this, then interrupt;  0 to stop counting
  TA0CTL = TASSEL_1 | MC_1;             // Clock source ACLK
  TA0CCTL1 = CCIE ;                      // Timer A interrupt enable

}



/**
 * Common stuff
 */ 


char transfer(char s) {

    unsigned char ret=0;
    int i;

    for(i=0;i<8;i++) {

        P1OUT |= SCK;
        __delay_cycles( 20 );

        ret <<= 1;
        // Put bits on the line, most significant bit first.
        if(s & 0x80) {
              P1OUT |= MOSI;
        } else {
              P1OUT &= ~MOSI;
        }
        s <<= 1;

        // Pulse the clock low and wait to send the bit.  According to
         // the data sheet, data is transferred on the rising edge.
        P1OUT &= ~SCK;
        __delay_cycles( 20 );

        // Send the clock back high and wait to set the next bit.  
        if (P1IN & MISO) {
          ret |= 0x01;
        } else {
          ret &= 0xFE;
        }

    }
    return ret; 

}

void zeroMem(unsigned char * p) {
     while (p != boundary) { *p++ = 0x00; }
     *p++ = 0x00; 
}


void copyMem(unsigned char * dest, unsigned char * src, unsigned int len) {
     if (len) {
         while (len--) {
             *dest++ = *src++;
         }
     }
}

