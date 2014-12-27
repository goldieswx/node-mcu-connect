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

#include "adce2.h"
#include "adce2-init.h"

#include "msp-utils.h"
#include "string.h"

inline void msp430SampleInputs(struct Sample * sample) {

	ADC10CTL0 &= ~ADC10IFG;
	ADC10CTL0 &= ~ENC;
	while (ADC10CTL1 & BUSY);             // Wait if ADC10 core is active
	ADC10SA = (unsigned int)sample->adc;  // Copies data in ADC10SA to unsigned int adc array
	ADC10CTL0 |= ENC + ADC10SC;           // Sampling and conversion start	

}

inline void msp430ResetUSCI() {

  UCA0CTL1 = UCSWRST;   
  UCB0CTL1 = UCSWRST;								// **Put state machine in reset**
  __delay_cycles(10);
  UCB0CTL0 |= UCCKPH  | UCCKPL | UCMSB | UCSYNC;	// 3-pin, 8-bit SPI slave
  UCB0CTL1 &= ~UCSWRST;								// **Initialize USCI state machine**

  while(IFG2 & UCB0RXIFG);							// Wait ifg2 flag on rx 

  IE2 |= UCB0RXIE;									// Enable USCI0 RX interrupt
  IFG2 &= ~UCB0RXIFG;

  UCB0RXBUF;
  UCB0TXBUF = 0x00;

}

inline void msp430StopUSCI() {

  UCA0CTL1 = UCSWRST;   
  UCB0CTL1 = UCSWRST;								// **Put state machine in reset**

}

inline void msp430NotifyNode(int level) {

	if (level) {
		P3OUT |= CS_NOTIFY_MASTER;
	} else {
		P3OUT &= ~CS_NOTIFY_MASTER;
	}
}

inline void msp430StartTimer(int delay) {

	TA0CCR0 = delay; 
	TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
	TA0CCTL1 = CCIE ;                      // Timer A interrupt enable

}

inline void msp430BitMaskPorts (char * bitMasks, struct IoConfig * ioConfig) {

	P1OUT |= (*bitMasks++ & ioConfig->P1DIR);
	P2OUT |= (*bitMasks++ & ioConfig->P2DIR);
	P3OUT |= (*bitMasks++ & ioConfig->P3DIR);
	P1OUT &= (*bitMasks++ | ~ioConfig->P1DIR);
	P2OUT &= (*bitMasks++ | ~ioConfig->P2DIR);
	P3OUT &= (*bitMasks++ | ~ioConfig->P3DIR);

}

inline void msp430InitializeClocks() {
	
  BCSCTL1 	= CALBC1_8MHZ;			// CPU Freq | 8MHz
  BCSCTL2  |= DIVS_3;				// SMCLK/DCO @ 1MHz
  BCSCTL3	= LFXT1S_2; 			// ACLK @ 10kHz

}


void initializeIOConfig(struct IoConfig * ioConfig) {

	struct flashConfig * flashIoConfig;
	flashIoConfig = (struct flashConfig*) IOCFG_HW_ADDR;

	if ((flashIoConfig->magic == 0x4573) && (flashIoConfig->_magic = 0x7354)) {
		memcpy(ioConfig,&flashIoConfig->ioConfig,sizeof(struct IoConfig));
	} else {
		ioConfig->P1DIR = 0x03;
		ioConfig->P1ADC = 0x04;
		ioConfig->P1REN = 0xFF;
		ioConfig->P1OUT = 0;
		ioConfig->P2DIR = 0x00;
		ioConfig->P2REN = 0xFF;
		ioConfig->P2OUT = 0;
		ioConfig->P3DIR = 0;
		ioConfig->P3REN = 0xFF;
		ioConfig->P3OUT = 0; 
	}

	ioConfig->P1DIR &= availP1 & ~ioConfig->P1ADC;
	ioConfig->P1ADC &= availP1;
	ioConfig->P1REN &= (availP1 & ~ioConfig->P1ADC & ~ioConfig->P1DIR);
	ioConfig->P1OUT &= (availP1 & ~ioConfig->P1ADC);
	ioConfig->P2DIR &= availP2;
	ioConfig->P2REN &= availP2 & ~ioConfig->P2DIR;
	ioConfig->P2OUT &= availP2;
	ioConfig->P3DIR &= availP3;
	ioConfig->P3REN &= availP3 & ~ioConfig->P3DIR;
	ioConfig->P3OUT &= availP3;

	P1DIR    = (P1DIR & (~availP1)) | ioConfig->P1DIR;
	P1SEL    = (P1SEL & (~availP1)) | ioConfig->P1ADC;
	P1SEL2   = (P1SEL2 & (~availP1));
	ADC10AE0 = ioConfig->P1ADC;
	P1REN    = (P1REN & (~availP1)) | ioConfig->P1REN;
	P1OUT    = (P1OUT & (~availP1)) | ioConfig->P1OUT;
	P2DIR    = (P2DIR & (~availP2)) | ioConfig->P2DIR;
	P2REN    = (P2REN & (~availP2)) | ioConfig->P2REN;
	P2OUT    = (P2OUT & (~availP2)) | ioConfig->P2OUT;
	P3DIR    = (P3DIR & (~availP3)) | ioConfig->P3DIR;
	P3REN    = (P3REN & (~availP3)) | ioConfig->P3REN;
	P3OUT    = (P3OUT & (~availP3)) | ioConfig->P3OUT;


}


void initializeUSCI() {

  P1DIR		|=	BIT5 | BIT7;				// BIT5 is Master Clock. BIT7 is MOSI
  P1DIR  	&= ~BIT6;						// BIT6 is MISO
  P1SEL		|=	BIT5 | BIT6 | BIT7; 		// Select BITS5 to 7 for USCI
  P1SEL2	|=  BIT5 | BIT6 | BIT7;

  P2DIR  &= ~CS_INCOMING_PACKET;			// CS_INCOMING PACKET is the Master signal triggering the SPI transfer.
  P2OUT  &= ~CS_INCOMING_PACKET;            // Outgoing,and no PullUp/Dn. Interrupt configured for Raising Edge.
  P2REN  &= ~CS_INCOMING_PACKET;            
  P2SEL  &= ~CS_INCOMING_PACKET;
  P2SEL2 &= ~CS_INCOMING_PACKET;

  P2IES = 0;
  P2IFG = 0;

  P3DIR  |= CS_NOTIFY_MASTER;
  P3OUT  &= ~CS_NOTIFY_MASTER;
  P3REN  &= ~CS_NOTIFY_MASTER;
  P3SEL  &= ~CS_NOTIFY_MASTER;
  P3SEL2 &= ~CS_NOTIFY_MASTER; 

}


void initializeADC(struct IoConfig * ioConfig) {

  ADC10AE0    = 0;
  ADC10CTL0   = 0;
  ADC10CTL1   = 0;
  ADC10MEM    = 0;
  ADC10DTC0   = 0;
  ADC10DTC1   = 0;

  ADC10CTL0 = SREF_0 + ADC10SHT_3 + MSC + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
  ADC10CTL1 = INCH_4 + CONSEQ_1 ;         // Channel 3, ADC10CLK/3
  ADC10AE0 = ioConfig->P1ADC;
  ADC10DTC1 = 5;                          // 5 conversions
}

void initializeTimer() {
  TA0R = 0;								// Reset timer counter.
  TA0CCR0 = 80;                         // Count to this, then interrupt; About 8ms. 
}
