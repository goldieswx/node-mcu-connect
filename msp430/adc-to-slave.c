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
 

void main(void)
{
        WDTCTL = WDTPW + WDTHOLD;        
        BCSCTL1 = CALBC1_1MHZ;           // DCOCTL = CALDCO_1MHZ;
        BCSCTL2 &= ~(DIVS_3);            // SMCLK/DCO @ 1MHz
        P1SEL |= BIT2;                   // ADC input pin P1.2

        ADC10CTL1 = INCH_2 + ADC10DIV_3 ;         // Channel 3, ADC10CLK/3
        ADC10CTL0 = SREF_0 + ADC10SHT_3 + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
        ADC10AE0 |= BIT2;    

        __enable_interrupt();           
 
        while(1)
        {
            __delay_cycles(100);                // ADC initializing
            ADC10CTL0 |= ENC + ADC10SC;          // Sampling and conversion start
            __bis_SR_register(CPUOFF + GIE);    // Low Power Mode 0 with interrupts enabled
            readValue = ADC10MEM;                // Assigns the value held in ADC10MEM to the integer called ADC_value
 
        }
}
 
interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
    LPM3_EXIT;
}
 
