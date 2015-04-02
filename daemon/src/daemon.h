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
	GNU General Public License for more details.F_memcpuy

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

#define word short
#define latch  RPI_GPIO_P1_22

#define BIT0  0x01
#define BIT1  0x02
#define BIT2  0x04
#define BIT3  0x08
#define BIT4  0x10
#define BIT5  0x20
#define BIT6  0x40
#define BIT7  0x80

#define BUFLEN 512
#define PORT 9930

#define MI_STATUS_QUEUED 0x03
#define MI_STATUS_TRANSFERRED 0x04
#define MI_STATUS_DROPPED 0x08


#define SNCC_SIGNAL_RECEIVED  0x05
#define SNCC_PREAMBLE_RECEIVED 0x06

#define MAX_TRANSFER_ERRORS_MESSAGE  5

struct UDPMessage {

	unsigned char data [MCOM_DATA_LEN] ;
	int   destination;

};


struct message {
	unsigned char data [MCOM_DATA_LEN] ;
	int   expectedChecksum;
	int   receivedChecksum;
	int   status;
	int   transferError;
	int   destination;
	unsigned long  noRetryUntil;
};


/* functions */
void 	debugMessage(struct message * m);
void 	_memcpy( void* dest, void *src, int len);
int 	dataCheckSum (unsigned char * req, int reqLen);
