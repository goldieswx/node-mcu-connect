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

#define __flashable__ __attribute__((section(".flashstart")))


#define availP1 (BIT0|BIT1|BIT2|BIT3|BIT4)	// max available usage of P1
#define availP2 (BIT1|BIT3|BIT4|BIT5|BIT6|BIT7)  // max available usage of P2
#define availP3 (BIT3|BIT4|BIT5|BIT6|BIT7)  // max available usage of P3

#define MOSI			BIT7 
#define MISO		 	BIT6
#define SCK				BIT5

#define CS_NOTIFY_MASTER  	BIT2   // External Interrupt INTR = P3.2
#define CS_INCOMING_PACKET  BIT2   // Master enable the line before sending  SIG = P2.2

#define MAX_ADC_CHANNELS 	5
#define NUM_PORTS_AVAIL  	3

#define PREAMBLE 			0xACAC

#define HIGH	1
#define LOW 	0

#define PWM_SET_DUTY_CYCLE 0X10
#define PWM_SET_VALUE 		0X20

#define PWM_INIT_SET_CHAN1 0X0001
#define PWM_INIT_UNSET_CHAN1 0X0002
#define PWM_INIT_SET_CHAN2 0X0004
#define PWM_INIT_UNSET_CHAN2 0X0008


#define FLASH_CUSTOM_RET_ERASE_FIRST 0x10
#define FLASH_CUSTOM_RET_NOT_IN_SEQUENCE 0x20
#define FLASH_CUSTOM_RET_TOO_LARGE 0x30


/* data struct */


struct CustomCmd {
	unsigned char CMDID;
	unsigned int  DATA[9];
};

struct CustomCmdDataPwmMessage { // max 18 bytes;

	unsigned int action; // set_duty_cycle_1,set_frequency_1  
	unsigned int data[4];

};

struct flashCustomCmd {
        int flashPosition;
        int data[9];
};


struct Sample {
        int     adc[MAX_ADC_CHANNELS];
        int     ports[3];
        int     trigger;
        int     sampled;        // true if sampled.
};

struct Return {
       int transferReturn;
       int code;
       union
        {
            uint16_t i[10];
            struct Sample s;
        } returnedData;
 
};


struct Exchange {
	int transferred;
	struct inBuffer {
		int    		  preamble;
		int 		  inData[10];
		int 		  checkSum;
	} inBuffer;
	int __padding_in[1];

	struct outBuffer {
		int    		  preamble;
		struct 		  Sample s;
		int 		  checkSum;
	} outBuffer;
	int __padding_out[1];

	int pointer;
	char * pIn;				// Floating Pointer to inBuffer:
	char * pOut;			// Floating Pointer to outBuffer;

};


void 	sample 				(struct Sample * sample);
void 	timer 				(int delay);
void 	fillSampleTrigger 	(struct Sample * new,struct Sample * old,struct IoConfig * ioConfig,struct Return * ret);

void    initializeReturn 	(struct Return * ret); 
void 	initializeSample 	(struct Sample * sample);
void 	initialize 			(struct IoConfig * ioConfig);
void 	signalNode 			(int level);
void 	fillExchange 		(struct Sample * sample,struct Exchange * exchange,struct IoConfig * ioConfig,struct Return * ret);

inline int incrementExchange(register int rx,struct Exchange * pExchange);
inline void getADCIoUsed 	(int ioADC, int * adcIoUsed);

void 	listen 				(struct Exchange * exchange);
void 	close 				();
void 	processExchange 	(struct Exchange * exchange,struct IoConfig * ioConfig,struct Return * ret);
