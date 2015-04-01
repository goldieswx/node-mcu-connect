
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


#define SIZEOF_MCOM_OUT_PAYLOAD  (MCOM_DATA_LEN+4*sizeof(char))
#define SIZEOF_MCOM_OUT_HEADER   (2*sizeof(char)+sizeof(word))
#define SIZEOF_MCOM_OUT_CHK      (2*sizeof(word))

#define MI_PREAMBLE 				0b10101100
#define MI_DOUBLE_PREAMBLE 			0xACAC
#define MI_CMD 						8714

#define MCOM_DATA_LEN 				20
#define MCOM_NODE_QUEUE_LEN 		10
#define MCOM_MAX_NODES 				16

struct McomInPacket {
	uint8_t 	preamble_1;
	uint8_t 	preamble_2;
	uint16_t 	cmd;
	uint8_t 	destinationCmd;
	uint8_t 	destinationSncc;
	uint8_t 	__reserved_1;
	uint8_t 	__reserved_2;
	uint8_t 	data[MCOM_DATA_LEN];
	uint16_t 	snccCheckSum;
	uint16_t 	chkSum;
};

struct McomOutPacket {
	uint8_t		 preamble_1;
	uint8_t		 preamble_2;
	uint16_t	 cmd;
	uint8_t 	signalMask2;
	uint8_t 	signalMask1;
	uint8_t 	__reserved_1;
	uint8_t 	__reserved_2;
	uint8_t 	data[MCOM_DATA_LEN];
	uint16_t 	snccCheckSum;
	uint16_t 	chkSum;
};

