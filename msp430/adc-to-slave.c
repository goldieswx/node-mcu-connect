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

/*
Proposed impl.

NODE TO IO
**********

======> SIG 
(======> INTR)
>>>>>>>>> MISO/MOSI/CLK [0x40,BFRLEN(Defined by NODE),xxxx,CHK_EXCHANGE] (clk driven by NODE)
(======> INTR)



IO TO NODE
**********

======> INTR
======> SIG
>>>>>>>>> MISO/MOSI/CLK [0x30,BFRLEN(Defined by IO),xxxx,CHK_EXCHANGE] (clk driven by NODE)
======> _INTR_

*/


#include "msp430g2553.h"
#include <legacymsp430.h>

unsigned int readValue=0;
unsigned int action=0;

unsigned int inHeader=0;

unsigned char bfr [20];
unsigned char store [20];
unsigned char adcData [16];

unsigned char * pbfr = 0;
unsigned char * pstore = 0;
unsigned char * bfrBoundary = 0;


#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

#define CS_NOTIFY_MASTER  BIT2   // External Interrupt INTR = P2.2
#define CS_INCOMING_PACKET  BIT2   // Master enable the line before sending  SIG = P3.2

#define PACKET_DAC 0b10010000

#define BEGIN_SAMPLE_DAC 0x02
#define CHECK_DAC 0x01

char transfer(char s) {
    while (!(IFG2 & UCB0TXIFG));
    UCB0TXBUF = s;
    return UCB0RXBUF;
}

void beginSampleDac() {

      action &= ~BEGIN_SAMPLE_DAC;
      ADC10CTL0 |= ENC + ADC10SC;          // Sampling and conversion start
  
      // wait for interrupt.   
}

void checkDAC() {
    action &= ~CHECK_DAC;
    __enable_interrupt();

    static int lastValue;

    readValue = ADC10MEM; // Assigns the value held in ADC10MEM to the integer called ADC_value

    //P2OUT |= BIT5;
    if (readValue < 750) {
      adcData[0] = 0xAB;
      adcData[3] = 0xcd;    
      adcData[2] = readValue >> 8;
      adcData[1] = readValue & 0xFF;

      if ((readValue > (lastValue+15)) || (readValue < (lastValue-15)))
      {
          P3OUT |= CS_NOTIFY_MASTER;
          lastValue = readValue;
          return;
      }
    }
    
   TA0CTL = TASSEL_1 | MC_1; 
   TA0CCTL1 = CCIE;

   return;
}


int main(void)
{
    WDTCTL = WDTPW + WDTHOLD;        
    BCSCTL1 = CALBC1_1MHZ;           // DCOCTL = CALDCO_1MHZ;
    BCSCTL2 &= ~(DIVS_3);            // SMCLK/DCO @ 1MHz
    
    P1DIR = BIT0 + BIT1 + BIT5 + BIT7;
    P1OUT = 0;

    P1SEL = BIT2;
       //P1SEL |= BIT1;                   // ADC input pin P1.2

    ADC10CTL1 = INCH_2 + ADC10DIV_0 ;         // Channel 3, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_0 + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
    ADC10AE0 |= BIT2;    


    P2DIR &= ~CS_INCOMING_PACKET;
    P2OUT &= 0;
    P3DIR &= 0;
    P3DIR |= CS_NOTIFY_MASTER;
    P3OUT &= 0,


    P2REN = 0;
    P2SEL = 0;
    P2SEL2 = 0;
    P2IES &= 0;

    P3REN = 0;
    P3SEL = 0;
    P3SEL2 = 0;

    P2IE  = CS_INCOMING_PACKET;
    P2IFG = 0;

   
    P1SEL |=    BIT5 + BIT6 + BIT7 ; 
    P1SEL2 =   BIT5 + BIT6 + BIT7 ;

  
    BCSCTL3 = LFXT1S_2; 

     UCB0CTL1 = UCSWRST;                       // **Put state machine in reset**
     __delay_cycles(10);
     UCB0CTL0 |= UCCKPH   + UCCKPL + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
     UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

     while(IFG2 & UCB0RXIFG);                  // Wait ifg2 flag on rx  (no idea what it does)
    //IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt
    UCB0TXBUF = 0x13;                         // We do not want to ouput anything on the line

    TA0R = 0;
    TA0CCR0 = 500;              // Count to this, then interrupt;  0 to stop counting
    TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
    TA0CCTL1 = CCIE ;                     // Timer A interrupt enable

    action = 0;
    bfrBoundary = bfr;
    pbfr = bfr;

    while(1)    {
        if (!action) {
          //  P1OUT |= BIT0;
            __bis_SR_register(LPM3_bits + GIE);
        }
        __enable_interrupt(); // process pending interrupts (between actions)
        __delay_cycles(100);
        __disable_interrupt(); 
        
        if(action & BEGIN_SAMPLE_DAC) {
            beginSampleDac();
            continue;
        }
        if(action & CHECK_DAC) {
            checkDAC();
            continue;
        }
    }
}
 
interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
    action |= CHECK_DAC;
    ADC10CTL0 &= ~ADC10IFG;
    __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM   
    return;

}
 
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

    if (pbfr) { UCB0TXBUF = *pbfr++; }
    if (pstore) { *pstore++ = UCB0RXBUF; }

    if (inHeader) {
          //check action.
         if (UCB0RXBUF == 0x01) {
            // master wants to interact with IOs
            //pbfr = bfr; // just send garbage to node
            //bfrBoundary = pbfr + 6; // 3x IOs &| mask 
         }  else {
        
            // we need to push data from adc.
            pbfr = adcData;
            //bfrBoundary = pbfr + 4; 
         }
         inHeader --;
    } 


    IFG2 &= ~UCB0RXIFG;   // clear current interrupt flag
    return;

}

interrupt(PORT2_VECTOR) P2_ISR(void) {

   if(P2IFG & CS_INCOMING_PACKET) {         // slave is ready to transmit, enable the SPI interrupt
       if (!(P2IES & CS_INCOMING_PACKET)) { // check raising edge
              
            UCB0TXBUF = 0x33;           // prepare first byte
            IE2 |= UCB0RXIE;              // enable spi interrupt
            inHeader = 1;
            IFG2 &= ~UCB0RXIFG;
            P2IES |= CS_INCOMING_PACKET; // switch to falling edge
            pstore = store;
          }
         else {

            if (store[0] == 0x01) {
                P1OUT &= (store[2] & (BIT0 + BIT1));
                P1OUT |= (store[1] & (BIT0 + BIT1));
            }
 
            IE2 &= ~UCB0RXIE;              // Disable SPI interrupt     
            IFG2 &= ~UCB0RXIFG;
            P2IES &= ~CS_INCOMING_PACKET;  // switch to raising edge

       }
    }

   P2IFG &= ~CS_INCOMING_PACKET;   // clear master notification interrupt flag        
   return;
}


interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

  TA0CTL = TACLR;  // stop & clear timer
  TA0CCTL1 &= 0;   // also disable timer interrupt & clear flag

  P3OUT &= ~CS_NOTIFY_MASTER;

/*  P3OUT ^= CS_NOTIFY_MASTER;
  P1OUT ^= (BIT0 + BIT1);   

  TA0CTL = TASSEL_1 | MC_1; 
  TA0CCTL1 = CCIE;
  return;
*/
  action |= BEGIN_SAMPLE_DAC;
  __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM
  return;
} 
