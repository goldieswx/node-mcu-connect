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
#include "msp-utils.h"



struct Sample {
	int 	adc[MAX_ADC_CHANNELS];
	int 	ports[3];
	int 	trigger;
	int     sampled;	// true if sampled.
}

struct Exchange {
	int transferred;
	struct inBuffer {
		int    		  preamble;
		int 		  inData[10];
		int 		  checkSum;
	};
	struct outBuffer {
		int    		  preamble;
		struct 		  Sample s;
		int 		  checkSum;
	};
}




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

		if (new.trigger || (INTERRUPT)) { // INTERRUPT here is 'node is interrupting'
			Exchange exchange;
			fillExchange 	(&sample,&exchange);		// prepare output buffers with sample.
			signalNode 		(HIGH);						// signal readyness to node 
			while (!INTERRUPT) __delay_cycles(100);
			listen 			(&exchange);				// start USCI and enable USCI interrupt.
			LOW_POWER_MODE	();							// go LPM and enable interrupts and wait.
			signalNode		(LOW);						// signal busyness to node
			close			();							// stop USCI.
			processExchange	(&exchange);				// process received buffer.
		}
	}
}


void fillExchange(Sample * sample,Exchange * exchange) {

	exchange->transferred = 0;
	setmem(&exchange->inBuffer,0,sizeof(exchange->inBuffer));
	memcpy(&exchange->outBuffer,sample,sizeof(exchange->inBuffer));

}


/**
 * function listen()
 * prepare in/out buffer
 * restart USCI
 * enable  USCI IE
 */
void listen(Exchange * exchange) {
	msp430ResetUSCI();
}

/**
 * function close()
 * stop USCI (and therefore USCI IE)
 * run received event if any.
 */
void close(ExchangeDataEvent * exchange) {
	msp430StopUSCI();
}

/**
 * sets/remove signal to node
 */
void signalNode (int level) {
	msp430NotifyNode(level);
}


void sample(SampleData * sample) {
	msp430SampleInputs(sample);
	sample->sampled = 1;
}


void timer(int delay) {
	msp430StartTimer(delay)
}


/** 
 * function fillSampleTrigger
 * compares new and old sample and find out what was triggered.
 */
int fillSampleTrigger(struct Sample * new,struct Sample * old,struct IoConfig * ioConfig) {

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
				   adcValue = new->adcData[i];
				   if ((adcValue > (old->adcData[i]+15)) || (adcValue < (old->adcData[i]-15))) {
						  old->adcData[i] = adcValue;
						  new->trigger |= 0x01 << i;
				   }
				}
			}
			for (i=0;i<NUM_PORTS_AVAIL;i++)	{
				if (old->ports[i] != new->ports[i])  new->trigger |= 0x01 << (0x05+i);
			}
		} else {
			new->trigger |= (1 << 8);
		}

		memcpy(old,new,sizeof(Sample));

}


int processExchange(struct Exchange * exchange) {


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


inline void getADCIoUsed (int ioADC, int * adcIoUsed) {
	/* Mark what input are being used. */
	for (i=0;i<MAX_ADC_CHANNELS;i++) {
		adcIoUsed[MAX_ADC_CHANNELS-i-1] = ioADC & 1; 
		ioADC >>=1;  
	}
}



void initializeSample(SampleDataEvent * sample) {

	setmem(&sample,sizeof(SampleData),0);
}

void initialize(struct IoConfig * ioConfig) {

	
  BCSCTL1 	= CALBC1_8MHZ;			// CPU Freq | 8MHz
  BCSCTL2  |= DIVS_3;				// SMCLK/DCO @ 1MHz
  BCSCTL3	= LFXT1S_2; 			// ACLK @ 10kHz

  initializeIOConfig(&ioConfig);
  //initializeUSCI();
  initializeTimer();

}