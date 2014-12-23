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


int main() {

	struct Sample 		   old;				// old sample to compare with (unsampled at boot)
	struct IoConfig        ioConfig;		// configuration of in/out ports

  	WDTCTL 	= WDTPW | WDTHOLD;	

	initializeSample(&old);
	initialize(&ioConfig);

	while(1) { 
		timer(300);										// wait
		LOW_POWER_MODE();								// go LPM and enable interrupts and wait.

		struct Sample new;						
		initializeSample(&new);							// initialize new sample structure

		sample(&new);									// start sampling ADC
		LOW_POWER_MODE();								// go LPM and enable interrupts and wait.

		fillSampleTrigger(&new,&old,&ioConfig);

		if (new.trigger || (NODE_INTERRUPT)) { 				// INTERRUPT read: 'node is interrupting'
			
			struct Exchange exchange;
			
			fillExchange 	(&new,&exchange);		// prepare output buffers with sample.
			signalNode 		(HIGH);						// signal readyness to node 
			while (!NODE_INTERRUPT) __delay_cycles(100);
			listen 			(&exchange);				// start USCI and enable USCI interrupt.
			LOW_POWER_MODE	();							// go LPM and enable interrupts and wait.
			signalNode		(LOW);						// signal busyness to node
			close			();							// stop USCI.
			processExchange	(&exchange,&ioConfig);				// process received buffer.
		}
	}
}


/**
 * DAC Sampling done.
 * Interrupt service routine.
 */

interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
	ADC10CTL0 &= ~ADC10IFG;
	__bic_SR_register_on_exit(LPM3_bits + GIE); 		// exit low power mode  
	return;

}


/**
 * USCI Byte received from SPI
 * Interrupt service routine.
 */

interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	static int tx;
	UCA0TXBUF = tx;

	tx = incrementExchange(UCA0RXBUF,NULL);

}

/**
 * Timer overflow
 * Interrupt service routine.
 */

interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

	TA0CTL = TACLR;										// stop & clear timer
	TA0CCTL1 &= 0;										// also disable timer interrupt & clear flag

	__bic_SR_register_on_exit(LPM3_bits + GIE); 		// exit low power mode  
	return;
}


inline int incrementExchange(register int rx,struct Exchange * pExchange) {

	static struct Exchange * exchange;
	if (pExchange) { exchange = pExchange; return 0x00; }
	if (++(exchange->pointer) > sizeof (struct Sample)) { return 0x00; }

	*(exchange->pIn) = rx;
	exchange->pIn++;


	return (*exchange->pOut++);

}


/**
 * function listen()
 * prepare in/out buffer
 * restart USCI
 * enable  USCI IE
 */
void listen(struct Exchange * exchange) {
	msp430ResetUSCI();
}

/**
 * function close()
 * stop USCI (and therefore USCI IE)
 * run received event if any.
 */
void close() {
	msp430StopUSCI();
}

/**
 * sets/remove signal to node
 */
void signalNode (int level) {
	msp430NotifyNode(level);
}


void sample(struct Sample * sample) {
	msp430SampleInputs(sample);
	sample->sampled = 1;
}


void timer(int delay) {
	msp430StartTimer(delay);
}

/**
 * fillExchange()
 * prepares exchange buffer and fills it with last sample
 */

void fillExchange(struct Sample * sample,struct Exchange * exchange) {

	memset(&exchange->inBuffer,0,sizeof(exchange->inBuffer));
	memcpy(&exchange->outBuffer.s,sample,sizeof(struct Sample));

	exchange->outBuffer.preamble = PREAMBLE;
	exchange->outBuffer.checkSum = 0x4567; // TODO;

	exchange->transferred 	= 0;
	exchange->pIn  			= (char*) &exchange->inBuffer;
	exchange->pOut 			= (char*) &exchange->outBuffer;

	exchange->__padding_in[0]	= 0;
	exchange->__padding_out[0] = 0;
	exchange->pointer 			= 0;

	incrementExchange(0,exchange); // sets exchange reference in intr

}

/** 
 * function fillSampleTrigger
 * compares new and old sample and find out what was triggered.
 */
void fillSampleTrigger(struct Sample * new,struct Sample * old,struct IoConfig * ioConfig) {

	register int i, adcValue;
    int usedADCIo[MAX_ADC_CHANNELS];

	new->trigger = 0;

	// Update ports state on new sample
	new->ports[0] = availP1 & (P1IN & ~ioConfig->P1DIR & ~ioConfig->P1ADC);
	new->ports[1] = availP2 & (P2IN & ~ioConfig->P2DIR);
	new->ports[2] = availP3 & (P3IN & ~ioConfig->P3DIR);

	getADCIoUsed(ioConfig->P1ADC,usedADCIo);

	if (old->sampled) {
		for (i=0;i<MAX_ADC_CHANNELS;i++) {
			if (usedADCIo[i]) {
			   adcValue = new->adc[i];
			   if ((adcValue > (old->adc[i]+15)) || (adcValue < (old->adc[i]-15))) {
					  old->adc[i] = adcValue;
					  new->trigger |= 0x01 << i;
			   }
			}
		}
		for (i=0;i<NUM_PORTS_AVAIL;i++)	{
			if (old->ports[i] != new->ports[i])  new->trigger |= 0x01 << (0x05+i);
		}
	} else {
		new->trigger |= (1 << 8); 	// we just booted up send initial config to node.
	}

	memcpy(old,new,sizeof(struct Sample));
}


void processExchange(struct Exchange * exchange,struct IoConfig * ioConfig) {

	char * pExchangeBuf = (char*)exchange->inBuffer.inData;
	switch (*pExchangeBuf) {
		case  0x55 :
		    flashConfig((struct IoConfig*)++pExchangeBuf);
		  	return;
		case 0x66 :
			msp430BitMaskPorts(++pExchangeBuf,ioConfig);
			return;
	}

}


inline void getADCIoUsed (int ioADC, int * adcIoUsed) {

	int i;	
	/* Mark what input are being used. */
	for (i=0;i<MAX_ADC_CHANNELS;i++) {
		adcIoUsed[MAX_ADC_CHANNELS-i-1] = ioADC & 1; 
		ioADC >>=1;  
	}
}



void initializeSample(struct Sample * sample) {

	memset(&sample,0,sizeof(struct Sample));
}

void initialize(struct IoConfig * ioConfig) {

	msp430InitializeClocks();
	initializeUSCI();
	initializeADC(ioConfig);
	initializeIOConfig(ioConfig);
	initializeTimer();

}
