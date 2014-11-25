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

Slave side.

On Power/Reset, goto 0


0  - Reset Watchdog 250ms  
   - "disable" data spi transfer 
   - P1IN can be whatever (not that we have any control on this anyway)
   - set P1OUT low.
   - Start Timer 500ticks (method A)
   - Upon Timer interrupt (method A), 
	run  1. signal = Either (ADC Check or Resend mode).
			 2. sample = Sample P1IN
			 3. transferTrigger = sample || signal
   - if transferTrigger 
		1aa. HOLd Watchdog
		1a. set P1OUT high
	1b. "reset USCI", prepare exchange buffer
		"enable data spi transfer"

	---
	2. 
	   on usci interrupt : 
		if data spi enabled, exchange buffers
		Reset watchdog 250ms.
		enable P1IN interrupt
	   on P1IN raising flag: if data spi enabled:
				verify checksums (then set Adc check or Resend mode).
				set P1IN in falling flag
	   then on P1IN falling flag:  
				set P1OUT at low.
				disable this interrupt
				goto 0
   - else 
	
	goto 0


Master side.

   Flags avail.   
	Clear to Send.  (signalMaster & signalMask) == 0;
   Actions avail.
	Signal Master   (signalMaster());
   Events avail.
	Process Buffer (from rbi) (mcomProcessBuffer, busy flag)

   On startup
	SIG to low.

   On (first) sync:
	
	Enable INTR interrupt on raising edge.
	if (INTR (is high) call checkDAC);
   
   On Process Buffer:
	
	Check INTR, 
		if low, set SIG high
			
		else (if high), start timer method A. .
		On: Timer method A. re-exec process buffer action.
   
   INTR interrupt : 
	if (raising edge)
		- clear this interrupt flag	

		/// separate action, checkDAC
		- delay (delta T).  
		- SIG To low
		- delay (delta T/2)
		- prepare and send exchange buffer containing CTS and inbuffer if avail.
		- SIG to hi
		- delay (delta T/2) (verify checksums and clear or not CTS/ set SignalMaster and outbuffer, 
				if failed to send, start timer method A (again))
		- SIG to lo.
*/

#include "msp430g2553.h"
#include <legacymsp430.h>

#include "adce.h"
#include "msp-utils.h"


static 	char	ioADCRead		[MAX_ADC_CHANNELS];  									// user usage of ADC inputs.
static	int  	adcData 		[MAX_ADC_CHANNELS+NUM_PORTS_AVAIL + CHECKSUM_SIZE ];	// sampling data buffer
static  int 	exchangeBuffer	[PREAMBLE_SIZE + MAX_ADC_CHANNELS+NUM_PORTS_AVAIL + CHECKSUM_SIZE+5]; //  5*2 security bytes just in case

volatile char *  pExchangeBuff=0;			// exchange buffer used for SPI transfers

static volatile int action;					// next set of actions to perform in the loop
static volatile int timerAction;			// action performed upon timer interrupt
static volatile int resample;				// boolean, whether we need to resample or resend
static volatile int initialTrigger; 		// intial trigger upon boot
static volatile unsigned int dataTrigger; 	// data trigger upon ADC Check
static volatile int transferTrigger; 		// data trigger || incoming_packet (whether a spi transfer must be triggered right now)


char availP1 = (BIT0|BIT1|BIT2|BIT3|BIT4);	// max available usage of P1
char availP2 = (BIT3|BIT4|BIT5|BIT6|BIT7);  // max available usage of P2
char availP3 = (BIT3|BIT4|BIT5|BIT6|BIT7);  // max available usage of P3

struct ioConfig ioConfig;					// user usage of all ports.
struct flashConfig * flashIoConfig = (struct flashConfig*) 0x0E000;  // address in flash rom of usage of all ports.


/**
 *  Main routine.
 *  Initialize default parameters. Then run action loop while processing interrupts.
 */

int main(void)
{

  WDTCTL 	= WDTPW | WDTHOLD;		// Hold watchdog 		
  BCSCTL1 	= CALBC1_8MHZ;			// CPU Freq | 8MHz
  BCSCTL2  |= DIVS_3;				// SMCLK/DCO @ 1MHz
  BCSCTL3	= LFXT1S_2; 			// ACLK @ 10kHz

  initConfig();
  initUSCI();
  initADC();
  resetUSCI();

  timerAction = 0;
  resample = 1;
  initialTrigger = 1;
  dataTrigger = 0;
  
  beginCycle();

  while(1)    {
	  if (!action) {
		  __bis_SR_register(LPM3_bits + GIE);
	  }
	  __enable_interrupt(); // process pending interrupts (between actions)
	  __delay_cycles(100);
	  __disable_interrupt(); 
	  
	  if(action & CHECK_DAC) {
		  checkDAC();
		  continue;
	  }
	  if(action & BEGIN_SAMPLE_DAC) {
		  beginSampleDac();
		  continue;
	  }
	  if(action & PROCESS_MSG) {
		  processMsg();
		  continue;
	  }
  }
}
 
/**
 * void beginCycle() 
 * Starts the main cycle.
 * 
 * [Wait][Sample/Resend][Register signals][Start Transfer]
 *
 */ 

void beginCycle() {

  initTimer();   //  initializes the timer (80 clicks at 10khz)
  
  P3OUT  &= ~CS_NOTIFY_MASTER;	// Release Node Interrupt
  timerAction = 0x01; 
  WDTCTL = WDT_ARST_16; 		// 16ms*3.2768 = 52ms
  startTimerSequence();  		// start timer and enable interrupt

}

/**
 * void checkDAC() 
 * Resend or resample, detect changes in input, update triggers and pack the buffer for sending.
 *
 */

void checkDAC() {

	action &= ~CHECK_DAC;  
	WDTCTL = WDTPW + WDTHOLD;

	static int lastValues[5];
	static char lastP1, lastP2, lastP3;

	char newP1,newP2,newP3;

	if (resample) {

		dataTrigger = 0;
		unsigned int i;
		int readValue;

		for (i=0;i<MAX_ADC_CHANNELS;i++) {
			if (ioADCRead[i]) {
			   readValue = adcData[i];
			   if ((readValue < 850) && ((readValue > (lastValues[i]+15)) || (readValue < (lastValues[i]-15)))) {
					  lastValues[i] = readValue;
					  dataTrigger |= 0x01 << i;
			   }
			}
		}

		newP1 = availP1 & (P1IN & ~ioConfig.P1DIR & ~ioConfig.P1ADC);
		newP2 = availP2 & (P2IN & ~ioConfig.P2DIR);
		newP3 = availP3 & (P3IN & ~ioConfig.P3DIR);

		dataTrigger |= (initialTrigger << 8);
		initialTrigger = 0;

		if (lastP1 != newP1) { dataTrigger |= 0x01 << 0x05; lastP1 = newP1; }
		if (lastP2 != newP2) { dataTrigger |= 0x01 << 0x06; lastP2 = newP2; }
		if (lastP3 != newP3) { dataTrigger |= 0x01 << 0x07; lastP3 = newP3; }

		int * pAdcData = &adcData[MAX_ADC_CHANNELS];

		if (dataTrigger) {
			*pAdcData++ = newP1 | newP2 << 0x08;
			*pAdcData++ = dataTrigger;
			*pAdcData++ = newP3;
		} else {
			adcData[6] = 0; //// buffer contains data to discard // trigger = 0
		}
	}

	transferTrigger = dataTrigger || (P2IN & CS_INCOMING_PACKET);

	if (transferTrigger) {

		P3OUT |= CS_NOTIFY_MASTER;

		/* prepare exchangeBuffer for sending */
		unsigned long int chkSum = 0;
		unsigned int i;
		for(i=0;i<MAX_ADC_CHANNELS+NUM_PORTS_AVAIL;i++) {
			chkSum += adcData[i];
		}

		_memcpy(exchangeBuffer+1,adcData,(MAX_ADC_CHANNELS+NUM_PORTS_AVAIL)*sizeof(int));
		exchangeBuffer[0] = PREAMBLE;
		exchangeBuffer[MAX_ADC_CHANNELS+NUM_PORTS_AVAIL+1] = chkSum & 0xFFFF;

		/* Prepare usci */
		WDTCTL = WDT_ARST_16;			 // release the dog
		pExchangeBuff = (char*)exchangeBuffer;
		resetUSCI();
		UCB0RXBUF;
		IFG2 &= ~UCB0RXIFG;
		UCB0TXBUF = * pExchangeBuff++; // now wait for usci interrupt 

	} else { 
		WDTCTL = WDTPW + WDTCNTCL;
		beginCycle();
	}
  return;
}


/**
 * void processMsg () 
 * Process the message sent by the node and execute the related action
 */

void processMsg () {
	action &= ~PROCESS_MSG;

	UCB0TXBUF = 0x00;
	WDTCTL = WDTPW + WDTCNTCL;
	if (exchangeBuffer[0] != PREAMBLE) { // just checking this for now. 
		//resample = 0;
		//initUSCI()
		//P1OUT ^= BIT1;
	} else {
		resample = 1;
		char * pXchBuf = (char*)&exchangeBuffer[PREAMBLE_SIZE];
		switch (*++pXchBuf) {
			case  0x55 :
			     flashConfig((struct ioConfig*)++pXchBuf);
			  break;
			case 0x66 :
				P1OUT |= (*++pXchBuf & ioConfig.P1DIR);
				P2OUT |= (*++pXchBuf & ioConfig.P2DIR);
				P3OUT |= (*++pXchBuf & ioConfig.P3DIR);
				P1OUT &= (*++pXchBuf | ~ioConfig.P1DIR);
				P2OUT &= (*++pXchBuf | ~ioConfig.P2DIR);
				P3OUT &= (*++pXchBuf | ~ioConfig.P3DIR);
			break;
		}
	}

	resample = 1;
	beginCycle();
}

/**
 * DAC Sampling done.
 * Interrupt service routine.
 */

interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
	action |= CHECK_DAC;
	ADC10CTL0 &= ~ADC10IFG;
	__bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM   
	return;

}

/**
 * USCI Byte received from SPI
 * Interrupt service routine.
 */
 
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	if (P3OUT & CS_NOTIFY_MASTER) {
		UCB0TXBUF = *pExchangeBuff;
		*pExchangeBuff = UCB0RXBUF;
		pExchangeBuff++;

		// and the last byte...
		if (P2IN & CS_INCOMING_PACKET) {
			P3OUT &= ~CS_NOTIFY_MASTER;
			action |= PROCESS_MSG;
			__bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM
		}
	} else {
		UCB0TXBUF = 0x00;
		IFG2 &= ~UCB0RXIFG;
	}
	return;
}


/**
 * Timer overflow
 * Interrupt service routine.
 */

interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

	TA0CTL = TACLR;  // stop & clear timer
	TA0CCTL1 &= 0;   // also disable timer interrupt & clear flag

	switch(timerAction) {
		case 0x01:
			if (resample)  {
			 action |= BEGIN_SAMPLE_DAC; 
			} else {
			 action |= CHECK_DAC;
			}
		break;
	}
	__bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM


	return;
} 



void initConfig() {

	if ((flashIoConfig->magic == 0x4573) && (flashIoConfig->_magic = 0x7354)) {
		_memcpy(&ioConfig,&flashIoConfig->ioConfig,sizeof(struct ioConfig));
	} else {
		ioConfig.P1DIR = 0x03;
		ioConfig.P1ADC = 0x04;
		ioConfig.P1REN = 0xFF;
		ioConfig.P1OUT = 0;
		ioConfig.P2DIR = 0x00;
		ioConfig.P2REN = 0xFF;
		ioConfig.P2OUT = 0;
		ioConfig.P3DIR = 0;
		ioConfig.P3REN = 0xFF;
		ioConfig.P3OUT = 0; 
	}

	ioConfig.P1DIR &= availP1 & ~ioConfig.P1ADC;
	ioConfig.P1ADC &= availP1;
	ioConfig.P1REN &= (availP1 & ~ioConfig.P1ADC & ~ioConfig.P1DIR);
	ioConfig.P1OUT &= (availP1 & ~ioConfig.P1ADC);
	ioConfig.P2DIR &= availP2;
	ioConfig.P2REN &= availP2 & ~ioConfig.P2DIR;
	ioConfig.P2OUT &= availP2;
	ioConfig.P3DIR &= availP3;
	ioConfig.P3REN &= availP3 & ~ioConfig.P3DIR;
	ioConfig.P3OUT &= availP3;

	P1DIR    = (P1DIR & (~availP1)) | ioConfig.P1DIR;
	P1SEL    = (P1SEL & (~availP1)) | ioConfig.P1ADC;
	P1SEL2   = (P1SEL2 & (~availP1));
	ADC10AE0 = ioConfig.P1ADC;
	P1REN    = (P1REN & (~availP1)) | ioConfig.P1REN;
	P1OUT    = (P1OUT & (~availP1)) | ioConfig.P1OUT;
	P2DIR    = (P2DIR & (~availP2)) | ioConfig.P2DIR;
	P2REN    = (P2REN & (~availP2)) | ioConfig.P2REN;
	P2OUT    = (P2OUT & (~availP2)) | ioConfig.P2OUT;
	P3DIR    = (P3DIR & (~availP3)) | ioConfig.P3DIR;
	P3REN    = (P3REN & (~availP3)) | ioConfig.P3REN;
	P3OUT    = (P3OUT & (~availP3)) | ioConfig.P3OUT;

	unsigned int i;
	char ioCfg = ioConfig.P1ADC;

	/* Mark what input are being used. */
	for (i=0;i<MAX_ADC_CHANNELS;i++) {
		ioADCRead[MAX_ADC_CHANNELS-i-1] = ioCfg & 1; 
		ioCfg >>=1;  
	}

}

void beginSampleDac() {

	action &= ~BEGIN_SAMPLE_DAC;

	ADC10CTL0 &= ~ENC;
	while (ADC10CTL1 & BUSY);             // Wait if ADC10 core is active
	ADC10SA = (unsigned int)adcData;      // Copies data in ADC10SA to unsigned int adc array
	ADC10CTL0 |= ENC + ADC10SC;           // Sampling and conversion start

	// Now wait for interrupt.   
}

inline void startTimerSequence() {

	TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
	TA0CCTL1 = CCIE ;                      // Timer A interrupt enable
}


void flashConfig(struct ioConfig * p) {

	WDTCTL = WDTPW + WDTHOLD;	// Hold watchdog.
	struct flashConfig buffer;
	buffer.magic = 0x4573;		// Surround Buffer with magic number.
	buffer._magic = 0x7354;      

	// Copy config buffer prior to writing it in flash
	_memcpy(&buffer.ioConfig,p,sizeof(struct ioConfig));

	// Do not write flash with the same existing buffer.
	if (_memcmp(&buffer,flashIoConfig,sizeof(struct flashConfig))) {
	  return;
	}

	//Do Update the Flash
	flash_erase((int*)flashIoConfig);
	flash_write((int*)flashIoConfig,(int*)&buffer,sizeof(struct flashConfig)/sizeof(int));

	WDTCTL = WDTHOLD; // reboot.
}


void resetUSCI() {

  UCA0CTL1 = UCSWRST;   
  UCB0CTL1 = UCSWRST;                       // **Put state machine in reset**
  __delay_cycles(10);
  UCB0CTL0 |= UCCKPH  | UCCKPL | UCMSB | UCSYNC;   // 3-pin, 8-bit SPI slave
  UCB0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

  while(IFG2 & UCB0RXIFG);                  // Wait ifg2 flag on rx 
  IE2 |= UCB0RXIE;                          // Enable USCI0 RX interrupt
  UCB0TXBUF = 0x00;                         // Keep MISO silent.

}


void initUSCI() {

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


void initADC() {

  ADC10AE0    = 0;
  ADC10CTL0   = 0;
  ADC10CTL1   = 0;
  ADC10MEM    = 0;
  ADC10DTC0   = 0;
  ADC10DTC1   = 0;

  ADC10CTL0 = SREF_0 + ADC10SHT_2 + MSC + ADC10ON + ADC10IE;  // Vcc,Vss as ref. Sample and hold 64 cycles
  ADC10CTL1 = INCH_4 + CONSEQ_1 ;         // Channel 3, ADC10CLK/3
  ADC10AE0 = ioConfig.P1ADC;
  ADC10DTC1 = 5;                          // 5 conversions
}

void initTimer() {
  TA0R = 0;								// Reset timer counter.
  TA0CCR0 = 80;                         // Count to this, then interrupt; About 8ms. 
}

