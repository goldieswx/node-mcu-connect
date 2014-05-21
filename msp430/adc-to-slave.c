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

unsigned int readValue=0;
unsigned int action=0;

unsigned char bfr [16];
unsigned char * pbfr;
unsigned char * bfrBoundary;


#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

#define CS_NOTIFY_MASTER  BIT3   // External Interrupt 
#define CS_INCOMING_PACKET  BIT4   // Master enable the line before sending

#define PACKET_DAC 0b10010000

#define BEGIN_SAMPLE_DAC 0x02
#define CHECK_DAC 0x01

char transfer(char s) {
    while (!(IFG2 & UCB0TXIFG));
    UCB0TXBUF = s;
    return UCB0RXBUF;
}

void beginSampleDac() {

    if (0==(P2IN & CS_INCOMING_PACKET))  {
      __delay_cycles(10);
      action &= ~BEGIN_SAMPLE_DAC;
      ADC10CTL0 |= ENC + ADC10SC;          // Sampling and conversion start
      // wait for interrupt.   
    }
}

void checkDAC() {
    action &= ~CHECK_DAC;

    static int lastValue;
    int chan = 7;
    readValue = ADC10MEM; // Assigns the value held in ADC10MEM to the integer called ADC_value
  
    if (readValue > 500) {

      int len = 4;
      bfr[0] = PACKET_DAC | len;
      bfr[3] = 0x2;    
      bfr[2] = readValue >> 8;
      bfr[1] = readValue & 0xFF;
      bfr[4] = 0xff; //readValue & 0xFF;
      bfrBoundary = &(bfr[5]);    

      if ((readValue > (lastValue+15)) || (readValue < (lastValue-15)))
      {
        pbfr = bfr;
        P2OUT |= CS_NOTIFY_MASTER;
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
    P1SEL |= BIT1;                   // ADC input pin P1.2

    ADC10CTL1 = INCH_1 + ADC10DIV_0 ;         // Channel 3, ADC10CLK/3
    ADC10CTL0 = SREF_0 + ADC10SHT_0 + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
    ADC10AE0 |= BIT1;    

    P1DIR = BIT0;
    P1OUT &= 0;

    P2DIR = CS_NOTIFY_MASTER | BIT5;
    P2DIR &= ~CS_INCOMING_PACKET;
    P2OUT &= 0;
    P2IE  = CS_INCOMING_PACKET;
    P2IES &= 0;
    P2REN |= CS_NOTIFY_MASTER;

    P1SEL =   BIT1 + BIT5 + BIT6 + BIT7 ; 
    P1SEL2 =   BIT5 + BIT6 + BIT7 ;

    UCB0CTL1 = UCSWRST;                       // **Put state machine in reset**
    UCB0CTL0 |= UCCKPH  + UCCKPL + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
    UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

    while(IFG2 & UCB0RXIFG);                  // Wait ifg2 flag on rx  (no idea what it does)
    //IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt
    //UCB0TXBUF = 0x32;                         // We do not want to ouput anything on the line
    UCB0TXBUF = 0x00;

    BCSCTL3 |= LFXT1S_2; 

    TA0R = 0;
    TA0CCR0 = 300; // 1000;// 32767;              // Count to this, then interrupt;  0 to stop counting
    TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
    TA0CCTL1 = CCIE ;                     // Timer A interrupt enable

    action = 0;
    bfrBoundary = bfr;
    pbfr = bfr;

    while(1)    {
        if (!action) {
            __bis_SR_register(LPM3_bits + GIE);
        }
        __enable_interrupt(); __delay_cycles(10); __disable_interrupt(); // process pending interrupts
        
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

   UCB0TXBUF = *pbfr++;      

      if (pbfr == bfrBoundary) {
            IE2 &= ~UCB0RXIE;              // Disable SPI interrupt     
            P2IFG &= ~CS_INCOMING_PACKET;   // clear master notification interrupt flag        
            P2OUT &= ~CS_NOTIFY_MASTER;    // Bring notify line low
           
            TA0CTL = TASSEL_1 | MC_1;  // enable timer.
            TA0CCTL1 = CCIE;

            __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM

            pbfr = bfr;
            bfrBoundary = bfr;

      }
  
  IFG2 &= ~UCB0RXIFG;   // clear current interrupt flag
  return;
  
}

interrupt(PORT2_VECTOR) P2_ISR(void) {

   if(P2IFG & CS_INCOMING_PACKET) {         // slave is ready to transmit, enable the SPI interrupt
       if (!(P2IES & CS_INCOMING_PACKET)) { // check raising edge
          if (P2OUT & CS_NOTIFY_MASTER) {   // did we notify master in the first place ?
              if (pbfr != bfrBoundary) {
                //IFG2 &= ~UCB0RXIFG;
                IE2 |= UCB0RXIE;              // enable spi interrupt
                UCB0TXBUF = *pbfr;           // prepare first byte
              }
          }
       } 
    }
   
   return;
}


interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

  TA0CTL = TACLR;  // stop & clear timer
  TA0CCTL1 &= 0;   // also disable timer interrupt & clear flag

  action |= BEGIN_SAMPLE_DAC;
  __bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM
  return;
} 
