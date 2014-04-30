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

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

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

    readValue = ADC10MEM; // Assigns the value held in ADC10MEM to the integer called ADC_value
    if (readValue > 256) {  
        P1OUT &= ~BIT0;
    } else {
        P1OUT |= BIT0;
        P2OUT &= ~BIT3;
    }
    if (readValue > 768) {  
        P1OUT |= BIT6;
        P2OUT |= BIT3;
    } else {
        P1OUT &= ~BIT6;
    }
    
    TA0CCR0 = 4000;    // Start counting as of now.
    A0CCTL1 = CCIE;   // Enable timer interrupt.
    
    transfer (readValue << 8);
    transfer (readValue & 0xFF);
    
}


int main(void)
{
        WDTCTL = WDTPW + WDTHOLD;        
        BCSCTL1 = CALBC1_1MHZ;           // DCOCTL = CALDCO_1MHZ;
        BCSCTL2 &= ~(DIVS_3);            // SMCLK/DCO @ 1MHz
        P1SEL |= BIT1;                   // ADC input pin P1.2

        ADC10CTL1 = INCH_1 + ADC10DIV_3 ;         // Channel 3, ADC10CLK/3
        ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
        ADC10AE0 |= BIT1;    

        P1DIR = BIT0;
        P1OUT &= 0;

        P2DIR = BIT3;
        P2OUT &= 0;

        P1SEL =   BIT1 + BIT5 + BIT6 + BIT7 ; 
        P1SEL2 =   BIT5 + BIT6 + BIT7 ;
  

        UCB0CTL1 = UCSWRST;                       // **Put state machine in reset**
        UCB0CTL0 |= UCCKPH + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
        UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

        while(IFG2 & UCB0RXIFG);                  // Wait ifg2 flag on rx  (no idea what it does)
        IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt
        //UCB0TXBUF = 0x32;                         // We do not want to ouput anything on the line
 
        BCSCTL3 |= LFXT1S_2; 

        TA0R = 0;
        TA0CCR0 = 1000;// 32767;                  // Count to this, then interrupt;  0 to stop counting
        TA0CTL = TASSEL_1 | MC_1;             // Clock source ACLK
        TA0CCTL1 = CCIE ;                      // Timer A interrupt enable

        action = 0;

        while(1)    {
            if (!action) {
                __bis_SR_register(LPM3_bits + GIE);
            }
            if(action & BEGIN_SAMPLE_DAC) {
                beginSampleDac();
            }
            if(action & CHECK_DAC) {
                checkDac();
            }
        }
}
 
interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
   action |= CHECK_DAC;
   LPM3_EXIT;
   
}
 
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

  UCB0TXBUF = 0x17;
  return;
}

interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

  TA0CCTL1 = ~CCIFG; // Clear TIMERA Interrupt   
  TA0R = 0;          // Reset counter to 0.
  TA0CCR0 = 0;       // stop counting
  
  action |= BEGIN_SAMPLE_DAC;
  LPM3_EXIT;
  
  return;
} 
