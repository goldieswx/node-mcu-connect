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


//#include "msp430g2553.h"
//#include <legacymsp430.h>

#include "string.h"

///// GLOBAL DECLARATIONS

#define nodeId 4
#define MI_RESCUE 0xacac0a22 //  0x220aacac

#define BIT2 0 
#define BIT1 1
#define BIT4 0

#define USCI_CONFIG_BITS_IN     (BIT2 | BIT4)
#define USCI_CONFIG_BITS_OUT    (BIT1)

#define USCI_CONFIG_BITS        (USCI_CONFIG_BITS_IN | USCI_CONFIG_BITS_OUT)

#define MASTER_REQUEST_SNCC(pc) \
                (((pc)->dataIn.destinationSncc == nodeId) && ((pc)->signalMaster))


#define MASTER_SENDING_MICMD(pc) \
                ((pc)->dataIn.destinationCmd == nodeId)

#define DO_CHECKSUM(pc,rx) \
                ((pc)->dataOut.chkSum += (rx))

#define SWITCH_LOW_POWER_MODE  //   __bis_SR_register(LPM3_bits + GIE)


#define PTR_END_OF_DESTINATION          (3)
#define PTR_END_OF_CHECKSUM             (PTR_END_OF_DESTINATION  + 22)
#define PTR_END_OF_PACKET               (PTR_END_OF_CHECKSUM + 4)

#define word short 

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
        int                   					outBufferChecksum;
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

};


int             initializePacketContainer(struct PacketContainer * packetContainer);

int             incrementPacket(const register int rx, struct PacketContainer  * packetContainer);
inline void 	rescue (const register int rx, struct PacketContainer * packetContainer);
inline int      inspectAndIncrement(const register int rx);

void            endOfHeaderEvent        (const register int rx, struct PacketContainer * packetContainer);
void            endOfDestinationEvent   (const register int rx, struct PacketContainer * packetContainer);
void            packetCheckSumEvent     (const register int rx, struct PacketContainer * packetContainer) ;
void            endOfPacketEvent        (const register int rx, struct PacketContainer * packetContainer);
void            noActionEvent           (const register int rx, const struct PacketContainer * packetContainer);
void 			prepareNextPacketCyle	(struct PacketContainer* packetContainer);

int main() {

    //initializeUSCI();
    inspectAndIncrement(0); // initialize static container.

    //SWITCH_LOW_POWER_MODE;

    //while(1);
    
    unsigned char b[] = {0xAC,0xAC,0x0A,0x22,0x04,0x04,0x00,0x00,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,0,0,210,0};

    int i = 0;
    for (i=0;i<sizeof(b);i++) {
       intr(b[i]);
    }




}

//interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

void intr(int UCA0RXBUF)
{
    static int tx;

    //UCA0TXBUF = tx;
    tx = inspectAndIncrement(UCA0RXBUF);
   


}


inline int inspectAndIncrement(const register int rx) {

	static struct PacketContainer packetContainer;

 
	if (packetContainer.initialized) {
                *(packetContainer.pIn) = rx;
		packetContainer.inspect(rx,&packetContainer);
		int tx;
		tx = incrementPacket(rx,&packetContainer);
		printf("TX : %x , RX: %x, pointer: %d, pin %x, pout %x \n",tx,rx,packetContainer.pointer,packetContainer.pIn,packetContainer.pOut);
		return tx;
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

	packetContainer->dataOut.data[0] = 1;
	packetContainer->dataOut.data[1] = 2;
	packetContainer->dataOut.data[2] = 3;
	packetContainer->dataOut.data[3] = 4;
	packetContainer->dataOut.data[4] = 5;

	packetContainer->outBufferChecksum = 0xFFAB;
	packetContainer->outBuffer[5] = 0xFFAB;
	packetContainer->signalMaster = 1;

	return 0;
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

		switch (packetContainer->pointer) {
			case PTR_END_OF_DESTINATION:
				packetContainer->inspect = (Inspector) &endOfDestinationEvent;
				break;
			case PTR_END_OF_CHECKSUM: // 1B before chk
				packetContainer->inspect = (Inspector) &packetCheckSumEvent;
				break;
			case PTR_END_OF_PACKET:
				packetContainer->inspect = (Inspector) &endOfPacketEvent;
				break;
			default:
				packetContainer->inspect = (Inspector) &noActionEvent;
		}

		return *(packetContainer->pOut++);

	} else  {
		rescue (rx,packetContainer);
		return 0;
	}
}


/**
 * function endOfHeaderEvent()
 */
void endOfHeaderEvent           (const register int rx,  struct  PacketContainer * packetContainer) {

	packetContainer->dataOut.signalMask1 = (packetContainer->signalMaster << nodeId);
        printf("End of Header\n");
}

/**
 * function endOfHeaderEvent()
 */
void endOfDestinationEvent      (const register int rx,  struct  PacketContainer * packetContainer) {


	if (MASTER_REQUEST_SNCC(packetContainer))    {
		packetContainer->pOut = (unsigned char*)packetContainer->outBuffer;                                     // Stream out buffer as of now.
              printf("Requested SNCC\n");
	}

	packetContainer->dataOut.snccCheckSum = packetContainer->outBufferChecksum;

	packetContainer->dataOut.chkSum = 0;                                                                            // Reset Checksum
	packetContainer->startMICMDCheckSum = MASTER_SENDING_MICMD(packetContainer);

        printf("End of Destination [%x]\n", packetContainer->startMICMDCheckSum  );

}

/**
 * function endOfHeaderEvent()
 */

void packetCheckSumEvent        (const register int rx,  struct  PacketContainer * packetContainer) {

packetContainer->startMICMDCheckSum = 0;
	    packetContainer->pOut = (unsigned char*)&packetContainer->dataOut.snccCheckSum  ; // Reset floating pointer to dataOut
//packetContainer->dataOut.snccCheckSum = 0x1234;
//packetContainer->dataOut.chkSum = 0x5678 ;

        printf("End of Checksum [%x]\n",packetContainer->dataOut.chkSum);

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

        printf("End of Packet\n");
	//prepareNextPacketCyle(packetContainer);

}

/**
 * function endOfHeaderEvent()
 *
 * dummy event inspector
 */
void noActionEvent                      (const register int rx, const struct  PacketContainer * packetContainer) {
       printf("No Action\n");
}



void prepareNextPacketCyle(struct PacketContainer* packetContainer) {

	packetContainer->pointer = 0;
	packetContainer->validChecksumCount++;
	packetContainer->inspect                = (Inspector)&noActionEvent;
	packetContainer->pIn                    = (unsigned char*)&packetContainer->dataIn;
	packetContainer->pOut                   = (unsigned char*)&packetContainer->dataOut;	
	packetContainer->incomplete = 0;

}

/**
 *      function rescue()
 *
 *  When the packet is in unsynced mode.
 *  Scan the incoming stream to find a MI_RESCUE long byte.
 *  When found, this is a candidate for a start of packet, thus update the packet state with synced.
 */

inline void rescue (const register int rx, struct PacketContainer * packetContainer) {

	unsigned char * rescuePtrSrc, *p;
	rescuePtrSrc = (unsigned char*) &packetContainer->dataIn + 3;
	packetContainer->pIn = rescuePtrSrc;

	p = rescuePtrSrc;

	*p = *(--rescuePtrSrc); p--;
	*p = *(--rescuePtrSrc); p--;
	*p = *(--rescuePtrSrc); p--;
	*p = rx;        


printf("Rescue : %x vs %x, %x %x\n",(*(long *)rescuePtrSrc));

	if ((*(long *)rescuePtrSrc) == MI_RESCUE) {

	    packetContainer->pIn = (unsigned char*) &packetContainer->dataIn + 4;
		printf("OK SYNC\n(pin=%x)",packetContainer->pIn);
	
		packetContainer->pOut = (unsigned char*) &packetContainer->dataOut + 5;
		packetContainer->pointer = 2;
		packetContainer->incomplete = 1;
		packetContainer->validChecksumCount = 0;
		packetContainer->synced = 1;
		endOfHeaderEvent(rx,packetContainer);
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










