/*
		node-mcu-connect . node.js UDP worderface for embedded devices.
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

#include "node.h"
#include "string.h" 
#include "hardware/hardware.h"
#include "stdio.h"


int main() {

    DISABLE_WATCHDOG();

	hwInitGlobal();
	hwInitializeUSCI();
	
	inspectAndIncrement(0,NULL); // initialize static container.

	struct PacketContainer * p = getPacketContainer(NULL);

	adceSetTrigger(1,0xFF); // enable trigger from all extensions. 

	while(1) {

		DISABLE_INTERRUPT;

		adceSetTrigger(1,0xFF); // enable trigger from all extensions. 
		if (!p->masterInquiryCommand) SWITCH_LOW_POWER_MODE;  
		adceSetTrigger(0,0xFF);
		
		ENABLE_INTERRUPT;
		
		while (p->signalMaster) { /* P2OUT ^= BIT7; */ DELAY(100); }

			// a few things are certain at this point
			// signalMaster is 0
		    // micmd is pending, and/or acde trigger is pendinng
			// in any case adce needs to be talked with so let's do.

		if (p->masterInquiryCommand) {
			int adceId = adceSignalCmd(p);  // 1 => P2OUT |= BIT0; 2 => P2OUT |= BIT1 
			switch (adceId) {
				case 1 : 
						while (!(EXT1_INTR)) { DELAY(5000); } // P1IN & BIT3
						break; 
				case 2 : 
						while (!(EXT2_INTR))  { DELAY(5000); } // P1IN & BIT3
						break;
			}
			adceServiceCmd(p,adceId);
			clearMasterInquiry(p); //only if processed correctly TODO
		} else {
			if ((EXT1_INTR) /*|| (EXT2_INTR)*/) {
				int adceId = adceSignalTriggerAny();
				adceServiceTrigger(p,adceId);
			}
		}
	}
	
	return 0;
}




inline word inspectAndIncrement(const register word rx,word * clearLP) {

	static struct PacketContainer packetContainer;
	if (packetContainer.initialized) {
		packetContainer.pClearLP = clearLP;
		*clearLP = 0;
		if (!packetContainer.incomplete) {
			prepareNextPacketCycle(&packetContainer);
		}

		if (packetContainer.synced){
			 *(packetContainer.pIn) = rx;
			 packetContainer.incomplete = 1; 
		 }

		packetContainer.inspect(rx,&packetContainer);

		return incrementPacket(rx,&packetContainer);
	}

	return initializePacketContainer(&packetContainer);

}

struct PacketContainer * getPacketContainer(struct PacketContainer * set) {

	static struct PacketContainer * packetContainer;
	if (!packetContainer && set) {
		packetContainer = set;
	}
	return packetContainer;

}


/**
 * function  initializePacketContainer()
 * Initialises/reset the pacektContrainer structure.
 *
 * Sets the incomplete flag, reset start of floating In and Out Pointers.
 * Sets the packet inspector to noActionEvent
 * The rest is Zeroed
 */

word initializePacketContainer(struct PacketContainer * packetContainer) {

	memset(packetContainer,0,sizeof(struct PacketContainer));
	packetContainer->inspect                = (Inspector)&noActionEvent;
	packetContainer->incomplete     = 1;
	packetContainer->pIn                    = (unsigned char*)&packetContainer->dataIn;
	packetContainer->pOut                   = (unsigned char*)&packetContainer->dataOut;
	packetContainer->initialized    = 1;
	packetContainer->pointer        = 1;

	getPacketContainer(packetContainer);
	return 0x00;
}

/**
 * function  incrementPacket()
 *
 * increment the in/out Packet floating pointers
 * update the packet packetInspector with respect from the packet pointer (.pointer);
 */

word incrementPacket(const register word rx, struct PacketContainer  * packetContainer) {

	if (packetContainer->synced) {

	packetContainer->pIn++;
	packetContainer->pointer++;

	if (packetContainer->startMICMDCheckSum) DO_CHECKSUM(packetContainer,rx);

		switch (packetContainer->pointer) {
			case PTR_END_OF_HEADER:
				packetContainer->inspect = (Inspector) &endOfHeaderEvent;
				break;
			case PTR_END_OF_DESTINATION:
				packetContainer->inspect = (Inspector) &endOfDestinationEvent;
				break;
			case PTR_END_OF_CHECKSUM_PRE:
				packetContainer->inspect = (Inspector) &packetPreCheckSumEvent;
				break;
			case PTR_END_OF_CHECKSUM: // 1B before chk
				packetContainer->inspect = (Inspector) &packetCheckSumEvent;
				break;
			case PTR_END_OF_PACKET:
				packetContainer->inspect = (Inspector) &endOfPacketEvent;
			break;
			case PTR_END_OF_PACKET2:
				packetContainer->inspect = (Inspector) &endOfPacketEvent2;
			break;
			case PTR_END_OF_RESERVED:
				packetContainer->inspect = (Inspector) &endOfReservedEvent;
				break;      
			default:
				packetContainer->inspect = (Inspector) &noActionEvent;
		}
	   
		return   *(packetContainer->pOut++);

	} else  {
		rescue (rx,packetContainer);
		if (packetContainer->synced)  return *(packetContainer->pOut++);
		return 0x00;
	}
}

inline void debugPacket(const char * c,const struct PacketContainer * p) {
	return;
}


void            endOfReservedEvent        (const register word rx, struct PacketContainer * packetContainer)
{

		 packetContainer->dataOut.chkSum = 0;                        // Reset Checksum
}


/**
 * function endOfHeaderEvent()
 */
void endOfHeaderEvent           (const register word rx,  struct  PacketContainer * packetContainer) {

	if (packetContainer->dataIn.preamble.i16 != MI_RESCUE) { 
		packetContainer->synced = 0; 
		return; 
	}  
	packetContainer->dataOut.signalMask1 =  (packetContainer->signalMaster << nodeId);

}

/**
 * function endOfHeaderEvent()
 */
void endOfDestinationEvent      (const register word rx,  struct  PacketContainer * packetContainer) {


	if (packetContainer->dataIn.cmd != MI_CMD) { 
		packetContainer->synced = 0; 
		return; 
	}

	if (MASTER_REQUEST_SNCC(packetContainer))    {
		packetContainer->pOut = (unsigned char*)packetContainer->outBuffer;                                     
		// Stream out buffer as of now.
		packetContainer->dataOut.snccCheckSum = packetContainer->outBufferChecksum;
	} else {
		packetContainer->dataOut.snccCheckSum = 0x00;
		//->dataOut buffer is always 0x00s (optimization to aviod conditions) so output is blanked in this condition.
	}
	packetContainer->startMICMDCheckSum = MASTER_SENDING_MICMD(packetContainer);

}

void packetPreCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer) {

	packetContainer->pOut = (unsigned char*)&packetContainer->dataOut.snccCheckSum   ; // Reset floating pointer to dataOut

}


/**
 * function endOfHeaderEvent()
 */

// micmd last byte, here we can have checksum calculated

void packetCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer) {


	if (packetContainer->startMICMDCheckSum) DO_CHECKSUM(packetContainer,rx); // append latest byte to chksum if needed
	packetContainer->startMICMDCheckSum = 0;


}

/**
 * function endOfHeaderEvent()
 // sent just after SNCC byte
 */

void endOfPacketEvent           (const register word rx, struct  PacketContainer * packetContainer) {


	if (MASTER_REQUEST_SNCC(packetContainer)) {
		if (packetContainer->dataIn.snccCheckSum == packetContainer->outBufferChecksum ) {
			packetContainer->signalMaster = 0;
		}
	}
	  
}

/**
 * function endOfHeaderEvent()
 */

void endOfPacketEvent2           (const register word rx, struct  PacketContainer * packetContainer) {


	if (packetContainer->clearMasterInquiry) {
		packetContainer->clearMasterInquiry = 0;
		packetContainer->masterInquiryCommand = 0;
	} else {
	
		if (packetContainer->masterInquiryCommand) {
			// previous unprocessed micmd, this can happen, if there was a signalmaster packet left (below) and we didnt wake (wake later)
			if (!packetContainer->signalMaster)	*packetContainer->pClearLP = 1;
		}

		if (MASTER_SENDING_MICMD(packetContainer)) {
			if (packetContainer->dataIn.chkSum == packetContainer->dataOut.chkSum) {
				packetContainer->masterInquiryCommand = 1;
				memcpy(packetContainer->inBuffer,packetContainer->dataIn.data,20);
				if (!packetContainer->signalMaster) {
					// we don't wake up now, but then wake later sice we're lpm.
					*packetContainer->pClearLP = 1;  /// clear low power flags on intr exit, 
					// except if there is a pending signal master, in that case don't wake up
					// since adce may then send an outward message -- and we'll have two messages
					// to transmit which we do not want.
					//P2OUT &= ~BIT6;
				} //else {
					//P2OUT |= BIT6;
				//}
			}
		}
	}

	packetContainer->dataIn.preamble.i16 = 0;
	
	packetContainer->incomplete = 0;  // mark packet as complete (to process)

	if (packetContainer->setSignalMaster) {
		packetContainer->signalMaster = 1;
		packetContainer->setSignalMaster = 0;
	}

	hwResetUSCI();

	if (packetContainer->synced && packetContainer->signalMaster) {
	        WAIT_COMM_RTS;
			TRIGGER_DEAMON_MESSAGE(nodeId);
	}
	  
}

/**
 * function noActionEvent()
 *
 * dummy event inspector
 */
void noActionEvent                      (const register word rx, const struct  PacketContainer * packetContainer) {
	
}


void clearMasterInquiry (struct PacketContainer * packetContainer) {

	if (!packetContainer) { packetContainer = getPacketContainer(NULL); }
	
	if (packetContainer->masterInquiryCommand) {
		if (packetContainer->incomplete == 0) { // packet complete, clear directly
			packetContainer->masterInquiryCommand = 0;
		} else { // a packet is being transmitted, clear indirectly
			packetContainer->clearMasterInquiry = 1;
		}
			
	}

}


void prepareNextPacketCycle(struct PacketContainer* packetContainer) {

	packetContainer->pointer = 1;
	packetContainer->validChecksumCount++;
	packetContainer->inspect                = (Inspector)&noActionEvent;
	packetContainer->pIn                    = (unsigned char*)&packetContainer->dataIn;
	packetContainer->pOut                   = (unsigned char*)&packetContainer->dataOut+2;    

}

/**
 *  function rescue()
 *
 *  When the packet is in unsynced mode.
 *  Scan the incoming stream to find a MI_RESCUE long byte.
 *  When found, this is a candidate for a start of packet, thus update the packet state with synced.
 */

inline void rescue (const register word rx, struct PacketContainer * packetContainer) {

	unsigned char * pNext, *p;
	p = (unsigned char*) &packetContainer->dataIn;
	packetContainer->pIn = p;

	pNext = p+1;

	*p = *(pNext); p++; 
	*p = rx;        

	if ((0xFFFF &  *(word *)packetContainer->pIn) ==  MI_RESCUE) {

		packetContainer->pIn = (unsigned char*) &packetContainer->dataIn + 1;
		packetContainer->pOut = (unsigned char*) &packetContainer->dataOut + 2;
		packetContainer->pointer = 2;
		packetContainer->incomplete = 1;
		packetContainer->validChecksumCount = 0;
		packetContainer->synced = 1;
		packetContainer->transmissionErrors = 0;
		endOfHeaderEvent(rx,packetContainer);
		incrementPacket(rx,packetContainer);

	} else {
		packetContainer->transmissionErrors++;
		if (packetContainer->transmissionErrors > 40) { RESET; }
	}
}


// ADCE RELATED STUFF

void adceSetTrigger(int state, int adceId) {

	hwExtSetClearInterrupt(state,adceId);

}



int adceSignalCmd(struct PacketContainer * p) {

	if (!p->masterInquiryCommand) return 1;

	int cmdId = *((char*)p->inBuffer); 
	switch(cmdId) {
		case 0x55:
		case 0x56:
		case 0x57:
			cmdId -= 0x55;
			*((char*)p->inBuffer) = 0x55;
			break;
		case 0x66:
		case 0x67:
		case 0x68:
			cmdId -= 0x66;
			*((char*)p->inBuffer) = 0x66;
			break;
		case 0x44:
		case 0x45:
		case 0x46:
			cmdId -= 0x44;
			*((char*)p->inBuffer) = 0x44;
			break;
		default:
			cmdId = 0;
	};


	int adceId = 1 << cmdId; 
    //1 => P2OUT |= BIT0; 2 => P2OUT |= BIT1 
	SIGNAL_EXT(adceId);
	return adceId;
}


int adceSignalTriggerAny() {

	int adceId = 0;
	if (EXT1_INTR) adceId = 1;
	if (EXT2_INTR) adceId = 2;

    SIGNAL_EXT(adceId);
	return adceId;
}


void adceServiceTrigger(struct PacketContainer * p, int adceId) {

	if (!adceId) return;

	p->outBufferChecksum = adceService(NULL,(unsigned char*) p->outBuffer,adceId);
	
	int trigger = p->outBuffer[8];

	if (trigger) {
		if (!p->incomplete) {
			if (p->synced) {
				TRIGGER_DEAMON_MESSAGE(nodeId);
			}
	    	p->signalMaster = 1;
	    } else {
		 	p->setSignalMaster = 1;
	    }
	}

}


int adceService(unsigned char * adceIn, unsigned char * adceOut, int adceId) {

	DELAY(1500);

	 uint16_t sum = 0;
	 int i,j = 0;
	
	
	 hwTransfer(0xAC);
	 hwTransfer(0xAC);

	 if (adceIn) { 	 
	 	 hwTransfer(adceIn[j++]);
		 hwTransfer(adceIn[j++]);

		 for (i=0;i<19;i++) {
		 	*adceOut = hwTransfer(adceIn[j++]);
		 	sum = crc16(sum,*adceOut);
		 	adceOut++;
		 }
	 } else {
	 	 hwTransfer(0);
		 hwTransfer(0);

		 for (i=0;i<19;i++) {
		 	*adceOut = hwTransfer(0);
		 	sum = crc16(sum,*adceOut);
		 	adceOut++;
		 }
 	 }

	//P2OUT &= ~BIT0;
	//P2OUT &= ~BIT1;
	STOP_SIGNAL_EXT(adceId);

	DELAY(500);

 	hwTransfer(0);
 	*adceOut = adceId;
	sum = crc16(sum,*adceOut);

	return sum;

}



void adceServiceCmd(struct PacketContainer * p, int adceId) {


	p->outBufferChecksum = adceService((unsigned char*) p->inBuffer,(unsigned char*) p->outBuffer,adceId);
	int trigger = p->outBuffer[8];

	if (trigger) {
		if (!p->incomplete) {
			if (p->synced) {
				TRIGGER_DEAMON_MESSAGE(nodeId);
			}
	    	p->signalMaster = 1;
	    } else {
		 	p->setSignalMaster = 1;
	    }
	}

}

