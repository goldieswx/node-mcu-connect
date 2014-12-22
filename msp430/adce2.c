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

int main() {

  	WDTCTL 	= WDTPW | WDTHOLD;	

	static SampleDataEvent sample;
	initialize(&sample);

	while(1) { 

		timer(300);	
		LOW_POWER_MODE();

		sample(&sample);
		LOW_POWER_MODE();

		getSampleChangeTrigger(sample);

		if (sample.trigger || (INTERRUPT)) { // INTERRUPT here is 'node is interrupting'
			ExchangeDataEvent exchange;
			listen(&sample,&exchange);
			signalNode(HIGH);
				while(!exchange.transferred)	LOW_POWER_MODE();
			close(&exchange);
			signalNode(LOW);
		}

	}
}


/**
 * function listen()
 * prepare in/out buffer
 * restart USCI
 * enable  USCI IE
 */
void listen(SampleDataEvent * sample,ExchangeDataEvent * exchange) {


}

/**
 * function close()
 * stop USCI (and therefore USCI IE)
 * run received event if any.
 */
void close() {

}


/**
 * sets/remove signal to node
 */
void signalNode (int level) {

}



void sample(SampleData & sample) {
	
	msp430SampleInputs(sample);

}


/** 
 * function hasSampleChanged
 */
int getSampleChangeTrigger(SampleDataEvent * sample) {


}


void initializeSampleEvent(SampleDataEvent * sample) {

	setmem(&sample,sizeof(SampleData),0);
}

void initialize(SampleDataEvent * sample) {

	
  BCSCTL1 	= CALBC1_8MHZ;			// CPU Freq | 8MHz
  BCSCTL2  |= DIVS_3;				// SMCLK/DCO @ 1MHz
  BCSCTL3	= LFXT1S_2; 			// ACLK @ 10kHz

  initializeSampleEvent(sample);

  initializeConfig();
  initializeUSCI();
  initializeTimer();


}