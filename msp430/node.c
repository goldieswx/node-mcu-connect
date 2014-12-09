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

///// GLOBAL DECLARATIONS

#define nodeId 4 

#define MASTER_REQUEST_SNCC(pc) (((pc)->dataIn.destinationSncc == nodeId) && ((pc)->signalMaster)) // if master choose us as sncc and if we signalled master
#define DO_CHECKSUM(pc,rx) ((pc)->dataOut.chksum += (rx))

static register unsigned int tx = 0;

struct PacketContainer {

    int 					pointer,				// Number from 0 to sizeof(McomInPacket)-1 representing the byte number being inspected.
	int 					incomplete,    			// When cleared, a packet has been sucessfully received from master.
	int 					synced,					// When set, the SPI stream is potentially in sync. (when checksumCount is set, it is fully in sync.)
	Inspector				inspect,				// Function pointer to the next/current packet inspector. 
	int 					validChecksumCount,			
	int     				signalMaster,			// When set, calls in a SNCC. 
	int     				masterInquiryCommand,   // When set means, the inBuffer needs to be transferred to an ADCE.
	int 					startMICMDCheckSum,     // When set, start Checksumming rx (MICMDs)

	int     				outBuffer[10],         	// Buffer to write to master (SNCCs)
	int                     outBufferChecksum,      // When writing SNCCs, checksum of the outBuffer
	int 					inBuffer[10],			// Buffer to write to ADCE


	
	struct McomInPacket 	dataIn,					// Raw data In
	struct McomOutPacket	dataOut, 				// Raw data Out

	unsigned char * 		pIn,					// Floating Pointer to dataIn
	unsigned char * 		pOut,					// Floating Pointer to dataOut

}

/////

int main() {
	
	initSPI();
	//initADCE();

	PacketContainer packetContainer;
	initializePacketContainer (&packetContainer);

	while(1) {	
		while(packetContainer.incomplete) {
			while(IFG2 & UCA0RXIFG) {
					// SPI byte received 
					UCA0TXBUF = tx;
					inspectAndIncrement(UCA0RXBUF,&packetContainer);
			}
		}
		transferPendingIO(); // transfer pending messages from/to adce if any
	}

}


inline void inspectAndIncrement(const register int rx, struct PacketContainer  * packetContainer) {
	packetContainer->inspect(rx,packetContainer);
	incrementPacket(rx,packetContainer);
}
	


/**
 * function  initializePacketContainer()
 * Initialises/reset the pacektContrainer structure.
 *
 * Sets the incomplete flag, reset start of floating In and Out Pointers.
 * Sets the packet inspector to noActionEvent
 * The rest is Zeroed 
 */

void initializePacketContainer(struct PacketContainer * packetContainer) {

	setmem(packetContainer,0,sizeof(struct PacketContainer));
	packetContainer->inspect 	= &noActionEvent;
	packetContainer->incomplete = 1;
	packetContainer->pIn	= &packetContainer->dataIn;
	packetContainer->pOut 	= &packetContainer->dataOut;
}


/**
 * function  incrementPacket()
 *
 * increment the in/out Packet floating pointers
 * update the packet packetInspector with respect from the packet pointer (.pointer);
 */

void incrementPacket(const register int rx, struct PacketContainer  * packetContainer) {

	if (packetContainer->synced) {

		packetContainer->pIn++;
		packetContainer->pOut++;
		packetContainer->pointer++;

		if (packetContainer->startCheckSum) DO_CHECKSUM(packetContainer,rx);

		switch (packetContainer->pointer) {
			case PTR_END_OF_HEADER:
				packetContainer->inspect = &endOfHeaderEvent;
			case PTR_END_OF_DESTINATION:
				packetContainer->inspect = &endOfDestinationEvent;
			case PTR_END_OF_CHECKSUM: // 1B before chk
		 		packetContainer->inspect = &packetCheckSumEvent;
			case PTR_END_OF_PACKET:
				packetContainer->inspect = &endOfPacketEvent
			default:
				packetContainer->inspect = &noActionEvent;
		}

	} else 	{
		rescue (rx,packetContainer);
	}

}


void endOfHeaderEvent		(const register int rx,  const  PacketContainer * packetContainer) {

	packetContainer->outPacket.signalMask1 = (packetContainer->signalMaster << nodeId);
}

void endOfDestinationEvent	(const register int rx,  const  PacketContainer * packetContainer) {


 	if (MASTER_REQUEST_SNCC(packetContainer))    {
        packetContainer->pOut = packetContainer->outBuffer;    					// Stream out buffer as of now.
    }

	packetContainer->dataOut.chkSum = 0; 										// Reset Checksum
    packetContainer->startMICMDCheckSum = MASTER_SENDING_MICMD(packetContainer);
	packetContainer->dataOut.snccChksum = packetContainer->outBufferChecksum; 

}

void packetCheckSumEvent	(const register int rx,  const  PacketContainer * packetContainer) {

	packetContainer->pOut = &packetContainer->dataOut[packetContainer->pointer]; // Reset floating pointer to dataOut

}


void endOfPacketEvent		(const register int rx, const  PacketContainer * packetContainer) {

	if (MASTER_REQUEST_SNCC(packetContainer)) {  
		if (packetContainer->dataIn.snccCheckSum == packetContainer->outBufferChecksum ) { 
			packetContainer->signalMaster = 0;
		}
	}

	if (MASTER_SENDING_MICMD(packetContainer)) { 
		if (packetContainer->dataIn.checkSum == packetContainer.dataOut.checkSum) {
			TRIGGER_ACTION_EXIT_LPM(MI_CMD_ACTION);
				//__bic_SR_register_on_exit(LPM3_bits); // exit LPM, keep global interrupts state      
		}
	}
	
}

void noActionEvent			(const register int rx, const  PacketContainer * packetContainer) {

	if (packetContainer->synced) {
		packetContainer->pIn = rx;
	}

}


/**
 *	function rescue()
 *
 *  When the packet is in unsynced mode.
 *  Scan the incoming stream to find a MI_RESCUE long byte.
 *  When found, this is a candidate for a start of packet, thus update the packet state with synced.
 */

inline void rescue (const register int rx,  const PacketContainer * packetContainer) {
	
		packetContainer->pInPacket = (unsigned char*) &packetContainer->inPacket;

		unsigned char * rescuePtrSrc = pInPacket;
		*pInPacket++ = *rescuePtrSrc++;
		*pInPacket++ = *rescuePtrSrc++;
		*pInPacket++ = *rescuePtrSrc;	
		*pInPacket = rx;

		if ((*(long *)&inPacket) == MI_RESCUE) {
			packetContainer->pointer = PTR_END_OF_HEADER;
			packetContainer->incomplete = 1;
			packetContainer->checksumCount = 0;
			endOfHeaderEvent(rx,packetContainer);
		}
}


void transferPendingIO() {
	
   if (signalMaster || outPacket.signalMask1) { // since we started receving the pkgs master was signalled
              UCA0TXBUF = 0x80 | currentNodeId;
   }

/*
	----- SCK
	----- SDI
	----- SDO
	----- A  (SIG, common)
	----- B  (INTR, particular)

   //OUT [A HIGH] [AC] [A LOW] [AC] [iid] [20b] [cso] [A HIGH] [cso] [A LOW]
   // IN 			[00]         [00] [000]	[20b] [csi]          [csi]

 	// cycle B pins
	if (_CNM_PIN & CS_NOTIFY_MASTER || pendingMICmd )  {
		_CIP_POUT |= CS_INCOMING_PACKET;
		while(1) {
			// A LOW
			__delay_cycles(100);
			// A HIGH
			if (transfer(0xAC) != 0xAC) continue;
			// A LOW
			if (transfer(0xAC) != 0xAC)	continue;
			//

			headIn	= transfer(head);
			chkIn 	= transfer(checkSum);
			buf[i] 	= transfer(buffer);

			break;
		}
		_CIP_POUT &= ~CS_INCOMING_PACKET;
		__delay_cycles(100);
	} */

}







void initSPI() {

  // UART A (main comm chanell with MASTER)
  //     (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 
	P1DIR	|= BIT1;
	P1DIR	&= ~(BIT2 | BIT4);
	P1SEL 	&=  ~(BIT1 | BIT2 | BIT4);
	P1SEL2 	&=  ~(BIT1 | BIT2 | BIT4);

	P1SEL 	|=  BIT1 | BIT2 | BIT4;
	P1SEL2 	|=  BIT1 | BIT2 | BIT4;

	UCA0CTL1 = UCSWRST;                       // **Put state machine in reset**
	__delay_cycles(10);
	UCA0CTL0 |= UCCKPH + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
	UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

	UCA0TXBUF = 0x00;                         // We do not want to ouput anything on the line

}




void initGlobal() {

  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer
  BCSCTL1 = CALBC1_12MHZ;
  DCOCTL = CALDCO_12MHZ;
  BCSCTL3 |= LFXT1S_2;        // Set clock source to VLO (low power osc for timer)

  P1REN &= 0; 
  P1OUT &= 0;
  P2REN &= 0;

  P2SEL2 &= 0;
  P2SEL &= 0;

  P2DIR |= BIT6 + BIT7;  
}


/**
 *  ADCE Related 
 */

void initADCE() {
	
  _CNM_PDIR &= ~CS_NOTIFY_MASTER ;
 // _CNM_PIE |=  CS_NOTIFY_MASTER ; 
  _CNM_PIFG &= ~CS_NOTIFY_MASTER;    
  _CNM_PIES &= ~CS_NOTIFY_MASTER ;  
  _CNM_PREN |=  CS_NOTIFY_MASTER;

  _CIP_PDIR |= CS_INCOMING_PACKET;
  _CIP_POUT &= ~CS_INCOMING_PACKET;  

  // UARTB 
  // Comm channel with extentions
  
  P1DIR |= BIT5 | BIT7;
  P1DIR &= ~BIT6;

  //power the extension
  //P1DIR |= BIT3;
  //P1OUT |= BIT3;
}



