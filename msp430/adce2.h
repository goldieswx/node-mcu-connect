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



#define availP1 = (BIT0|BIT1|BIT2|BIT3|BIT4);	// max available usage of P1
#define availP2 = (BIT3|BIT4|BIT5|BIT6|BIT7);  // max available usage of P2
#define availP3 = (BIT3|BIT4|BIT5|BIT6|BIT7);  // max available usage of P3

#define MOSI			BIT7 
#define MISO		 	BIT6
#define SCK				BIT5

#define CS_NOTIFY_MASTER  	BIT2   // External Interrupt INTR = P3.2
#define CS_INCOMING_PACKET  BIT2   // Master enable the line before sending  SIG = P2.2

#define PROCESS_MSG 		  0x04      //  Schedule processMsg() Task
#define BEGIN_SAMPLE_DAC 	0x02      //  Schedule beginSampleDAC() Task
#define CHECK_DAC 			  0x01      //  Schedule checkDAC() Task

#define MAX_ADC_CHANNELS 	5
#define NUM_PORTS_AVAIL  	3

#define PREAMBLE_SIZE 		0x01
#define CHECKSUM_SIZE 		0x01

#define PREAMBLE 			0xACAC

/* data struct */

struct ioConfig {
	unsigned char P1DIR;
	unsigned char P1ADC;
	unsigned char P1REN;
	unsigned char P1OUT;
	unsigned char P2DIR;
	unsigned char P2REN;
	unsigned char P2OUT;
	unsigned char P3DIR;
	unsigned char P3REN;
	unsigned char P3OUT;
};

struct flashConfig {
	unsigned int  magic;
	struct ioConfig ioConfig;
	unsigned int  _magic;
};
