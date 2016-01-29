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


#include "msp-utils.h"
#include "adce2.h"
#include "adce2-init.h"
#include "string.h"


int main() {

	struct Sample 		   old;				// old sample to compare with (unsampled at boot)
	struct IoConfig        ioConfig;		// configuration of in/out ports

  	WDTCTL 	= WDTPW | WDTHOLD;	

	initializeSample(&old);
	memset(&ioConfig,0,sizeof(ioConfig));
	initialize(&ioConfig);

	signalNode 		(LOW);

	int i;	for (i=0;i<10
		;i++) __delay_cycles(10000);

	

	while(1) { 
		
		timer(30);										// wait
		LOW_POWER_MODE();								// go LPM and enable interrupts and wait.
		struct Sample new;						
		initializeSample(&new);							// initialize new sample structure

		sample(&new);									// start sampling ADC
		LOW_POWER_MODE();								// go LPM and enable interrupts and wait.

		fillSampleTrigger(&new,&old,&ioConfig);
	
		if (new.trigger || (NODE_INTERRUPT)) { 				// INTERRUPT read: 'node is interrupting'
			struct Exchange exchange;
		
			//while(1);

			fillExchange 	(&new,&exchange,&ioConfig);		// prepare output buffers with sample.
			signalNode 		(HIGH);						// signal readyness to node 

			while (!NODE_INTERRUPT) __delay_cycles(100);
	
			//P3OUT |= BIT4 | BIT3;


			listen 			(&exchange);				// start USCI and enable USCI interrupt.
			LOW_POWER_MODE	();							// go LPM and enable interrupts and wait.
			
			//while (1);
			signalNode		(LOW);						// signal busyness to node
			close			();							// stop USCI.
			
			//
			processExchange	(&exchange,&ioConfig);				// process received buffer.
			//while(1);
		}
	}
}


/**
 * DAC Sampling done.
 * Interrupt service routine.
 */

interrupt(ADC10_VECTOR) ADC10_ISR (void) { 
   
	ADC10CTL0 &= ~ADC10IFG;
	__bic_SR_register_on_exit(ADCE_LPM_BITS + GIE); 		// exit low power mode  
	return;

}


/**
 * USCI Byte received from SPI
 * Interrupt service routine.
 */

interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	static int tx;
	UCB0TXBUF = tx;

	tx = incrementExchange(UCB0RXBUF,NULL);

	//P3OUT &= ~BIT3;
		

	if (!NODE_INTERRUPT) {
		__bic_SR_register_on_exit(ADCE_LPM_BITS + GIE); 		// exit low power mode  
	} else {
		__bis_SR_register_on_exit(ADCE_LPM_BITS); 
	}
	return;
}

/**
 * Timer overflow
 * Interrupt service routine.
 */

/*interrupt(TIMER0_A1_VECTOR) ta1_isr(void) {

	TA0CTL = TACLR;										// stop & clear timer
	TA0CCTL1 &= 0;										// also disable timer interrupt & clear flag

	__bic_SR_register_on_exit(LPM3_bits + GIE); 		// exit low power mode  
	return;
}*/


/**
 * WDT Timer
 * Interrupt service routine.
 */
interrupt(WDT_VECTOR) WDT_ISR(void) {

	WDTCTL = WDTPW+WDTHOLD;
   	__bic_SR_register_on_exit(ADCE_LPM_BITS + GIE); 
	IFG1&=~WDTIFG;

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

void fillExchange(struct Sample * sample,struct Exchange * exchange,struct IoConfig * ioConfig) {

	memset(&exchange->inBuffer,0,sizeof(exchange->inBuffer));
	memcpy(&exchange->outBuffer.s,sample,sizeof(struct Sample));

	//memcpy(&exchange->outBuffer.s,ioConfig,10);

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


	if (new->trigger) { memcpy(old,new,sizeof(struct Sample));   }
}


void pwmInitializeChannels(struct CustomCmdDataPwmMessage * msg) {

	/*
		data[0] bit0 => set/reset TA0
		data[0] bit1 => set/reset TA1
      	data[1] = duty cycle TA0
     	data[2] = duty cycle TA1

		// adce hardware pwm ettings
		//3.5 3.6  ta0.1 ta0.2
		//2.1 3.3  ta1.1 ta1.2
	*/

		if (msg->data[1]) TA0CCR0 = msg->data[1] ;		// 3000 Duty cycle
		if (msg->data[2]) TA1CCR0 = msg->data[2] ;		// 3000 Duty cycle

		if (msg->data[0] & PWM_INIT_SET) {
    		TA0CCTL1 = OUTMOD_7; 		// CCR1 reset/set
      		TA0CTL = TASSEL_2 + MC_1;	// SMCLK, up mode
    	} else {
			 TA0CTL = TASSEL_2 + MC_0; // stop timer
 		}

		if (msg->data[1] & PWM_INIT_SET) {
    		TA1CCTL1 = OUTMOD_7; 		// CCR1 reset/set
    		TA1CTL = TASSEL_2 + MC_1;	// SMCLK, up mode
    	} else {
			 TA1CTL = TASSEL_2 + MC_0; // stop timer
 		}

}


void pwmSetChannelValues(struct CustomCmdDataPwmMessage * msg) {

	// data 0  TA0CCR1
	// data 1  TA0CCR2
	// data 2  TA1CCR1
	// data 4  TA1CCR2

	if (msg->data[0] != 0xFFFF) TA0CCR1 = msg->data[0];
	if (msg->data[1] != 0xFFFF) TA0CCR2 = msg->data[1];
	if (msg->data[2] != 0xFFFF) TA1CCR1 = msg->data[2];
	if (msg->data[3] != 0xFFFF) TA1CCR2 = msg->data[3];


}

void customCmdProcessPwmMessage(struct CustomCmdDataPwmMessage * msg) {

	static int initializedChannels;

	switch (msg->action) {
		case PWM_SET_DUTY_CYCLE:
			//setDutyCycle intializes nonzero duty cycle channels
			pwmInitializeChannels(msg);
			break;
		case PWM_SET_VALUE:
			pwmSetChannelValues(msg);
			break;
	};
	if (msg->action ==)

	TA0CCR1 = msg->data[0];

}


void customCmd(struct CustomCmd* cmd) {

	switch(cmd->CMDID) {
		case 0x01 : 
			customCmdProcessPwmMessage((struct CustomCmdDataPwmMessage *) cmd->DATA);


	}

}


void processExchange(struct Exchange * exchange,struct IoConfig * ioConfig) {

	char * pExchangeBuf = (char*)exchange->inBuffer.inData;
	switch (*pExchangeBuf) {
		case  0x44 : 
			customCmd((struct CustomCmd*)++pExchangeBuf);
            return;
		case  0x55 :
		    flashConfig((struct IoConfig*)++pExchangeBuf);
		  	return;
		case  0x66 :
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

	memset(sample,0,sizeof(struct Sample));
}

void initialize(struct IoConfig * ioConfig) {

	msp430InitializeClocks();
	initializeIOConfig(ioConfig);
	initializeUSCI();
	initializeADC(ioConfig);
	initializeTimer();

}
