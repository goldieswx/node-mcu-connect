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

#include "string.h" 

///// GLOBAL DECLARATIONS
#define LONG long
#define word short 

#define nodeId 					4
#define MI_RESCUE				0xacac 
#define MAX_TRANSMISSION_ERRORS 40

#define MASTER_REQUEST_SNCC(pc) \
				(((pc)->dataIn.destinationSncc == nodeId) && ((pc)->signalMaster))

#define MASTER_SENDING_MICMD(pc) \
				((pc)->dataIn.destinationCmd == nodeId)

#define DO_CHECKSUM(pc,rx) \
				((pc)->dataOut.chkSum += (rx))

#define SWITCH_LOW_POWER_MODE   __bis_SR_register(LPM3_bits + GIE)


#define PTR_END_OF_HEADER               (2)     // Header ends after preamble
#define PTR_END_OF_DESTINATION          (6)     // Desgination ends after destinationSNCC
#define PTR_END_OF_RESERVED             (8)     
#define PTR_END_OF_CHECKSUM             (PTR_END_OF_DESTINATION  + 21)
#define PTR_END_OF_CHECKSUM_PRE         (PTR_END_OF_CHECKSUM - 1) 
#define PTR_END_OF_PACKET               (PTR_END_OF_CHECKSUM + 2) // out lags 2 bytes behind in (therefore 4-2 = 2).


struct McomInPacket {
  unsigned word                                 preamble;
  unsigned word                                 cmd;
  unsigned char                                 destinationCmd;
  unsigned char                                 destinationSncc;
  unsigned char                                 __reserved_1;
  unsigned char                                 __reserved_2;
  unsigned char                                 data[20];
  unsigned word                                 snccCheckSum;
  unsigned word                                 chkSum;
};

struct McomOutPacket {
  unsigned word                                 preamble;
  unsigned word                                 cmd;
  unsigned char                                 signalMask2;
  unsigned char                                 signalMask1;
  unsigned char                                 __reserved_1;
  unsigned char                                 __reserved_2;
  unsigned char                                 data[20];
  unsigned word                                 snccCheckSum;
  unsigned word                                 chkSum;
};

typedef void (*Inspector) ( int rx, void  * packetContainer);

struct PacketContainer {

		int                                         pointer;
		// Number from 0 to sizeof(McomInPacket)-1 representing the byte number being inspected.
		int                                     incomplete;
		// When cleared, a packet has been sucessfully received from master.
		int                                     synced;
		// When set, the SPI stream is potentially in sync. (when checksumCount is set, it is fully in sync.)
		Inspector                               inspect;
		// Function pointer to the next/current packet inspector.
		int                                     validChecksumCount;
		int                                     signalMaster;
		// When set, calls in a SNCC.
		int                                     masterInquiryCommand;
		// When set means, the inBuffer needs to be transferred to an ADCE.
		int                                     startMICMDCheckSum;
		// When set, start Checksumming rx (MICMDs)

		int                                     outBuffer[10];
		// Buffer to write to master (SNCCs)
		int                                     outBufferChecksum;
		// When writing SNCCs, checksum of the outBuffer
		int                                     inBuffer[10];
		// Buffer to write to ADCE

		struct McomInPacket     dataIn;
		// Raw data In
		struct McomOutPacket    dataOut;
		// Raw data Out

		unsigned char *                 pIn;
		// Floating Pointer to dataIn
		unsigned char *                 pOut;
		// Floating Pointer to dataOut
		int                                     initialized;
		// This structure needs to be initialized.
		int                                     transmissionErrors;

};



/* */

int             	initializePacketContainer(struct PacketContainer * packetContainer);
int             	incrementPacket(const register int rx, struct PacketContainer  * packetContainer);
inline void    		rescue (const register int rx, struct PacketContainer * packetContainer);
inline int      	inspectAndIncrement(const register int rx);
inline Inspector 	getNextInspector(const register int pointer);
void           		prepareNextPacketCycle   (struct PacketContainer* packetContainer);

/* Register Event inspectors */ 

void	endOfHeaderEvent        (const register int rx, struct PacketContainer * packetContainer);
void	endOfDestinationEvent   (const register int rx, struct PacketContainer * packetContainer);
void	endOfPacketEvent        (const register int rx, struct PacketContainer * packetContainer);
void	endOfReservedEvent      (const register int rx, struct PacketContainer * packetContainer);
void	packetCheckSumPreEvent 	(const register int rx, struct PacketContainer * packetContainer);
void	packetCheckSumEvent     (const register int rx, struct PacketContainer * packetContainer);
void	noActionEvent           (const register int rx, const struct PacketContainer * packetContainer);

/*  MSP SPECIFIC */

void	initGlobal();
void	initializeUSCI();

#define USCI_CONFIG_BITS_IN     (BIT2 | BIT4)
#define USCI_CONFIG_BITS_OUT    (BIT1)
#define USCI_CONFIG_BITS        (USCI_CONFIG_BITS_IN | USCI_CONFIG_BITS_OUT)

#define RESET WDTCTL = WDTHOLD


/* */

int main() {

 	initGlobal();
	initializeUSCI();
	inspectAndIncrement(0); // initialize static container.

	SWITCH_LOW_POWER_MODE;

	while(1);
	return 0;
}

interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	static int tx;
	UCA0TXBUF = tx;
	tx = inspectAndIncrement(UCA0RXBUF);

}


inline int inspectAndIncrement(const register int rx) {

	static struct PacketContainer packetContainer;
	if (packetContainer.initialized) {

		if (!packetContainer.incomplete) prepareNextPacketCycle(&packetContainer);

		if (packetContainer.synced) {
			 *(packetContainer.pIn) = rx;
			 packetContainer.incomplete = 1; 
		 }

		packetContainer.inspect(rx,&packetContainer);
		return incrementPacket(rx,&packetContainer);
	}

	return initializePacketContainer(&packetContainer);

}

/**
 * function  initializePacketContainer()
 * Initialises/reset the pacektContrainer structure.
 *
 * Sets the incomplete flag, reset start of floating In and Out Pointers.
 * Sets the packet inspector to noActionEvent
 * The rest is Zeroed
 */
int initializePacketContainer(struct PacketContainer * packetContainer) {

	memset(packetContainer,0,sizeof(struct PacketContainer));
	packetContainer->inspect                = (Inspector)&noActionEvent;
	packetContainer->incomplete     = 1;
	packetContainer->pIn                    = (unsigned char*)&packetContainer->dataIn;
	packetContainer->pOut                   = (unsigned char*)&packetContainer->dataOut;
	packetContainer->initialized    = 1;

	return 0x00;
}

/**
 * function  incrementPacket()
 *
 * increment the in/out Packet floating pointers
 * update the packet packetInspector with respect from the packet pointer (.pointer);
 */

int incrementPacket(const register int rx, struct PacketContainer  * packetContainer) {

	if (packetContainer->synced) {

		packetContainer->pIn++;
		packetContainer->pointer++;

		if (packetContainer->startMICMDCheckSum) DO_CHECKSUM(packetContainer,rx);

		packetContainer->inspect = getNextInspector(packetContainer->pointer);
		return   *(packetContainer->pOut++);

	} else  {
		rescue (rx,packetContainer);
		if (packetContainer->synced)  return *(packetContainer->pOut++);
		return 0x00;
	}
}


inline Inspector getNextInspector(const register int pointer) {

		switch (pointer) {
			case PTR_END_OF_HEADER:
				return (Inspector) &endOfHeaderEvent;
			case PTR_END_OF_DESTINATION:
				return (Inspector) &endOfDestinationEvent;
			case PTR_END_OF_CHECKSUM: 
				return (Inspector) &packetCheckSumEvent;
			case PTR_END_OF_PACKET:
				return (Inspector) &endOfPacketEvent;
			case PTR_END_OF_RESERVED:
				return (Inspector) &endOfReservedEvent;
			case PTR_END_OF_CHECKSUM_PRE:
				return (Inspector) &packetCheckSumPreEvent;
			default:
				return (Inspector) &noActionEvent;
		};

}

/**
 * function endOfReservedEvent()
 */
void endOfReservedEvent (const register int rx, struct PacketContainer * packetContainer)
{
		 packetContainer->dataOut.chkSum = 0;                       
}

/**
 * function endOfHeaderEvent()
 */
void endOfHeaderEvent (const register int rx,  struct  PacketContainer * packetContainer) {

	if (((*(int *)&packetContainer->dataIn)) != MI_RESCUE) { 
		packetContainer->synced = 0; 
		return; 
	}  
	packetContainer->dataOut.signalMask1 = (packetContainer->signalMaster << nodeId);
}

/**
 * function endOfHeaderEvent()
 */
void endOfDestinationEvent (const register int rx,  struct  PacketContainer * packetContainer) {

	if (MASTER_REQUEST_SNCC(packetContainer))    {
		packetContainer->pOut = (unsigned char*)packetContainer->outBuffer;                                     
		// Stream out buffer as of now.
		packetContainer->dataOut.snccCheckSum = packetContainer->outBufferChecksum;
	} else {
		packetContainer->dataOut.snccCheckSum = 0x00;
	}
	packetContainer->startMICMDCheckSum = MASTER_SENDING_MICMD(packetContainer);

}

/**
 * function endOfHeaderEvent()
 */

void packetCheckSumEvent (const register int rx,  struct  PacketContainer * packetContainer) {

	if (packetContainer->startMICMDCheckSum) DO_CHECKSUM(packetContainer,rx); // append latest byte to chksum if needed
	packetContainer->startMICMDCheckSum = 0;

}

void packetCheckSumPreEvent (const register int rx,  struct  PacketContainer * packetContainer) {

	packetContainer->pOut = (unsigned char*)&packetContainer->dataOut.snccCheckSum   ; // Reset floating pointer to dataOut

}

/**
 * function endOfHeaderEvent()
 */

void endOfPacketEvent           (const register int rx, struct  PacketContainer * packetContainer) {

	if (MASTER_REQUEST_SNCC(packetContainer)) {
		if (packetContainer->dataIn.snccCheckSum == packetContainer->outBufferChecksum ) {
			packetContainer->signalMaster = 0;
		}
	}

	if (MASTER_SENDING_MICMD(packetContainer)) {
		if (packetContainer->dataIn.chkSum == packetContainer->dataOut.chkSum) {
			packetContainer->masterInquiryCommand = 1;
		}
	}

	packetContainer->incomplete = 0;  // mark packet as complete (to process)
	  
}

/**
 * function endOfHeaderEvent()
 *
 * dummy event inspector
 */
void noActionEvent                      (const register int rx, const struct  PacketContainer * packetContainer) {
}



void prepareNextPacketCycle(struct PacketContainer* packetContainer) {

	packetContainer->pointer = 0;
	packetContainer->validChecksumCount++;
	packetContainer->inspect                = (Inspector)&noActionEvent;
	packetContainer->pIn                    = (unsigned char*)&packetContainer->dataIn;
	packetContainer->pOut                   = (unsigned char*)&packetContainer->dataOut;    

}

/**
 *  function rescue()
 *
 *  When the packet is in unsynced mode.
 *  Scan the incoming stream to find a MI_RESCUE long byte.
 *  When found, this is a candidate for a start of packet, thus update the packet state with synced.
 */

inline void rescue (const register int rx, struct PacketContainer * packetContainer) {

	unsigned char * pNext, *p;
	p = (unsigned char*) &packetContainer->dataIn;
	packetContainer->pIn = p;

	pNext = p+1;

	*p = *(pNext); p++; 
	*p = rx;        

	if ((*(int *)packetContainer->pIn) ==  MI_RESCUE) {

		packetContainer->pIn = (unsigned char*) &packetContainer->dataIn + 2;
		packetContainer->pOut = (unsigned char*) &packetContainer->dataOut + 3;
		packetContainer->pointer = 2;
		packetContainer->incomplete = 1;
		packetContainer->validChecksumCount = 0;
		packetContainer->synced = 1;
		packetContainer->transmissionErrors = 0;
		endOfHeaderEvent(rx,packetContainer);

	} else {
		packetContainer->transmissionErrors++;
		if (packetContainer->transmissionErrors > MAX_TRANSMISSION_ERRORS) RESET;
	}
}

/**
 * function endOfHeaderEvent()
 */


void transferPendingIO() {

 //  if (signalMaster || outPacket.signalMask1) { // since we started receving the pkgs master was signalled
 //             UCA0TXBUF = 0x80 | currentNodeId;
 //  }

/*
		----- SCK
		----- SDI
		----- SDO
		----- A  (SIG, common)
		----- B  (INTR, particular)

   //OUT [A HIGH] [AC] [A LOW] [AC] [iid] [20b] [cso] [A HIGH] [cso] [A LOW]
   // IN                        [00]         [00] [000] [20b] [csi]          [csi]

		// cycle B pins
		if (_CNM_PIN & CS_NOTIFY_MASTER || pendingMICmd )  {
				_CIP_POUT |= CS_INCOMING_PACKET;
				while(1) {
						// A LOW
						__delay_cycles(100);
						// A HIGH
						if (transfer(0xAC) != 0xAC) continue;
						// A LOW
						if (transfer(0xAC) != 0xAC)     continue;
						//

						headIn  = transfer(head);
						chkIn   = transfer(checkSum);
						buf[i]  = transfer(buffer);

						break;
				}
				_CIP_POUT &= ~CS_INCOMING_PACKET;
				__delay_cycles(100);
		} */

}



/**
 * function intializeUSCI()
 * UART A (main comm channel with MASTER)
 *    (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 */

void initializeUSCI() {

		P1DIR   |=  USCI_CONFIG_BITS_OUT;
		P1DIR   &=  ~USCI_CONFIG_BITS_IN;
		P1SEL   &=  ~USCI_CONFIG_BITS;
		P1SEL2  &=  ~USCI_CONFIG_BITS;
		P1SEL   |=  USCI_CONFIG_BITS;
		P1SEL2  |=  USCI_CONFIG_BITS;

		UCA0CTL1 = UCSWRST;                                             // Reset USCI
		__delay_cycles(10);
		UCA0CTL0 |= UCCKPH + UCMSB + UCSYNC;    // 3-pin, 8-bit SPI slave
		UCA0CTL1 &= ~UCSWRST;                                   // Enable USCI
		UCA0TXBUF = 0x00;                                               // SIlent output
		while(IFG2 & UCA0RXIFG);
		IE2 |= UCA0RXIE;                                                // Enable USCI0 RX interrupt
}




void initGlobal() {

		WDTCTL = WDTPW + WDTHOLD;                               // Stop watchdog timer
		BCSCTL1 = CALBC1_8MHZ;
		DCOCTL = CALDCO_8MHZ;
		BCSCTL3 |= LFXT1S_2;                                    // Set clock source to VLO (low power osc for timer)

		P1REN &= 0;
		P1OUT &= 0;
		P1DIR = 0xFF;

		P2REN &= 0;
		P2SEL2 &= 0;
		P2SEL &= 0;

		P2DIR |= BIT6 + BIT7;                                   // Debug LEDs
		P2OUT = 0;
}
