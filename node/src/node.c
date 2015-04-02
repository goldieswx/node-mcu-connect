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

#ifdef MSP
#include "msp430g2553.h"
#include <legacymsp430.h>

#define LONG long
#define word int 

#define SWITCH_LOW_POWER_MODE   __bis_SR_register(LPM3_bits + GIE)
#define ENABLE_INTERRUPT 		__enable_interrupt()
#define DISABLE_INTERRUPT 		__disable_interrupt()
#define RESET 					WDTCTL = WDTHOLD
#define DISABLE_WATCHDOG(a)  	WDTCTL = WDTPW + WDTHOLD    
  
#else

#include "stdio.h"

#define BIT0  0x01
#define BIT1  0x02
#define BIT2  0x04
#define BIT3  0x08
#define BIT4  0x10
#define BIT5  0x20
#define BIT6  0x40
#define BIT7  0x80

#define LONG int
#define word short int

#define SWITCH_LOW_POWER_MODE   
#define ENABLE_INTERRUPT 		
#define DISABLE_INTERRUPT
#define RESET 					
#define DISABLE_WATCHDOG(a)  	    

#endif

#include "string.h" 

///// GLOBAL DECLARATIONS

#define printf(a)				

#define nodeId 					5
#define MI_RESCUE				0xacac 
#define MI_CMD                  0x220a

#define USCI_CONFIG_BITS_IN     (BIT2 | BIT4)
#define USCI_CONFIG_BITS_OUT    (BIT1)
#define USCI_CONFIG_BITS        (USCI_CONFIG_BITS_IN | USCI_CONFIG_BITS_OUT)

#define MASTER_REQUEST_SNCC(pc) \
				(((pc)->dataIn.destinationSncc == nodeId) && ((pc)->signalMaster))

#define MASTER_SENDING_MICMD(pc) \
				(((pc)->dataIn.destinationCmd == nodeId) && (!(pc)->masterInquiryCommand))

#define DO_CHECKSUM(pc,rx) \
				((pc)->dataOut.chkSum += (rx))




#define PTR_END_OF_HEADER               (2)     // Header ends after preamble
#define PTR_END_OF_DESTINATION          (PTR_END_OF_HEADER + 5)     // Desgination ends after destinationSNCC
#define PTR_END_OF_RESERVED             (PTR_END_OF_DESTINATION + 1)     
#define PTR_END_OF_CHECKSUM_PRE         (PTR_END_OF_DESTINATION  + 20)
#define PTR_END_OF_CHECKSUM             (PTR_END_OF_DESTINATION  + 21)
#define PTR_END_OF_PACKET               (PTR_END_OF_CHECKSUM + 2) // out lags 2 bytes behind in (therefore 4-2 = 2).
#define PTR_END_OF_PACKET2              (PTR_END_OF_CHECKSUM + 4) // out lags 2 bytes behind in (therefore 4-2 = 2).



#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5


struct McomInPacket {
  unsigned word                                 preamble;
  unsigned word                                 cmd;
  unsigned char                                 destinationCmd;
  unsigned char                                 destinationSncc;
  unsigned char                                 __reserved_1;
  unsigned char                                 __reserved_2;
  unsigned char                                 data[20];
  word                                 snccCheckSum;
  word                                 chkSum;
};

struct McomOutPacket {
  unsigned word                                 preamble;
  unsigned word                                 cmd;
  unsigned char                                 signalMask2;
  unsigned char                                 signalMask1;
  unsigned char                                 __reserved_1;
  unsigned char                                 __reserved_2;
  unsigned char                                 data[20];
  word                                 snccCheckSum;
  word                                 chkSum;
};

typedef void (*Inspector) ( word rx, void  * packetContainer);

struct PacketContainer {

		word                                     pointer;
		// Number from 0 to sizeof(McomInPacket)-1 representing the byte number being inspected.
		word                                     incomplete;
		// When cleared, a packet has been sucessfully received from master.
		word                                     synced;
		// When set, the SPI stream is potentially in sync. (when checksumCount is set, it is fully in sync.)
		Inspector                               inspect;
		// Function pointer to the next/current packet inspector.
		word                                     validChecksumCount;
		word                                     signalMaster;
		// When set, calls in a SNCC.
		word                                     masterInquiryCommand;
		// When set means, the inBuffer needs to be transferred to an ADCE.
		word                                     startMICMDCheckSum;
		// When set, start Checksumming rx (MICMDs)

		word                                     outBuffer[10];
		// Buffer to write to master (SNCCs)
		word                                     outBufferChecksum;
		// When writing SNCCs, checksum of the outBuffer
		word                                     inBuffer[10];
		// Buffer to write to ADCE

		struct McomInPacket     dataIn;
		word __padding; // output padding must be 0 (used when tx overflows)
		// Raw data In
		struct McomOutPacket    dataOut;
		// Raw data Out
		word ___padding; // output padding must be 0 (used when tx overflows)
		unsigned char *                 pIn;
		// Floating Pointr to dataIn
		unsigned char *                 pOut;
		// Floating Pointer to dataOut
		word                                     initialized;
		// This structure needs to be initialized.
		word                                     transmissionErrors;
		// clear  masterInquiryCommand bit on next packet end.
		word 								clearMasterInquiry;
		word *								pClearLP; // clear low power flag on rx interrupt
		word 								setSignalMaster;

};

word             initializePacketContainer(struct PacketContainer * packetContainer);

word             incrementPacket(const register word rx, struct PacketContainer  * packetContainer);
inline void     rescue (const register word rx, struct PacketContainer * packetContainer);
inline word      inspectAndIncrement(const register word rx, word* pClearLP);

void            endOfHeaderEvent        (const register word rx, struct PacketContainer * packetContainer);
void            endOfDestinationEvent   (const register word rx, struct PacketContainer * packetContainer);
void            endOfPacketEvent        (const register word rx, struct PacketContainer * packetContainer);
void            endOfReservedEvent      (const register word rx, struct PacketContainer * packetContainer);
void 			endOfPacketEvent2       (const register word rx, struct  PacketContainer * packetContainer);
void            packetCheckSumEvent     (const register word rx, struct PacketContainer * packetContainer);
void 			packetPreCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer);


void            noActionEvent           (const register word rx, const struct PacketContainer * packetContainer);
void            prepareNextPacketCycle   (struct PacketContainer* packetContainer);

void 			initGlobal();
void 			initializeUSCI();
word 			transfer(word s);
void 			clearMasterInquiry (struct PacketContainer * packetContainer);
struct 			PacketContainer * getPacketContainer(struct PacketContainer * set);

void 			adceSetTrigger(int state, int adceId);
int 			adceSignalCmd(struct PacketContainer * p); 
int 			adceSignalTriggerAny();
void 			adceServiceTrigger(struct PacketContainer * p, int adceId);
int 			adceService(unsigned char * adceIn, unsigned char * adceOut, int adceId);
void 			adceServiceCmd(struct PacketContainer * p, int adceId);



#ifndef MSP

int rxInt (word UCA0RXBUF);

#endif

inline void msp430ResetUSCI() {


//  setupUSCIPins(1);

  UCA0CTL1 = UCSWRST;   
  UCB0CTL1 = UCSWRST;								// **Put state machine in reset**


  __delay_cycles(10);
  UCA0CTL0 |= UCCKPH  | UCMSB | UCSYNC;	// 3-pin, 8-bit SPI slave
  UCA0CTL1 &= ~UCSWRST;								// **Initialize USCI state machine**

  while(IFG2 & UCA0RXIFG);							// Wait ifg2 flag on rx 

  IE2 |= UCA0RXIE;									// Enable USCI0 RX interrupt
  IFG2 &= ~UCA0RXIFG;

  UCA0RXBUF;
  UCA0TXBUF = 0x00;

}


word main() {

    DISABLE_WATCHDOG();

	initGlobal();
	initializeUSCI();
	inspectAndIncrement(0,NULL); // initialize static container.

	struct PacketContainer * p = getPacketContainer(NULL);

	adceSetTrigger(1,0xFF); // enable trigger from all extensions. 

	while(1) {

		DISABLE_INTERRUPT;
		adceSetTrigger(1,0xFF); // enable trigger from all extensions. 
		if (!p->masterInquiryCommand) SWITCH_LOW_POWER_MODE;  
		adceSetTrigger(0,0xFF);
		ENABLE_INTERRUPT;
		
		while (p->signalMaster) { P2OUT ^= BIT7; __delay_cycles(100); }

			// a few things are certain at this point
			// signalMaster is 0
		    // micmd is pending, and/or acde trigger is pendinng
			// in any case adce needs to be talked with so let's do.

		if (p->masterInquiryCommand) {
			int adceId = adceSignalCmd(p);  // 1 => P2OUT |= BIT0; 2 => P2OUT |= BIT1 
			switch (adceId) {
				case 1 : 
						while (!(P1IN & BIT3)) { __delay_cycles(5000); } // P1IN & BIT3
						break; 
				case 2 : 
						while (!(P2IN & BIT4))  { __delay_cycles(5000); } // P1IN & BIT3
						break;
			}
			adceServiceCmd(p,adceId);
			clearMasterInquiry(p); //only if processed correctly TODO
		} else {
			if ((P1IN & BIT3) /*|| (P2IN & BIT4)*/) {
				int adceId = adceSignalTriggerAny();
				adceServiceTrigger(p,adceId);
			}
		}
	}
	return 0;
}




#ifdef MSP
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {
	static word tx;
	word clearLP;
	
   if (UCA0STAT & UCOE) {
      WDTCTL = WDTHOLD;
   }

	UCA0TXBUF = tx;
	tx = inspectAndIncrement(UCA0RXBUF,&clearLP);
	if (clearLP) { __bic_SR_register_on_exit(LPM3_bits + GIE); };
	return;

}


interrupt(PORT1_VECTOR) p1_isr(void) { 

	P1IE  &= ~BIT3;
	
	P1IFG = 0;
 	__bic_SR_register_on_exit(LPM3_bits + GIE); 		// exit low power mode  
 	return;
  
} 

interrupt(PORT2_VECTOR) p2_isr(void) { 

	P2IE  &= ~BIT4;
	
	P2IFG = 0;
 	__bic_SR_register_on_exit(LPM3_bits + GIE); 		// exit low power mode  
 	return;
  
} 

#endif


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

	if (packetContainer->dataIn.preamble != MI_RESCUE) { 
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
					P2OUT &= ~BIT6;
				} else {
					P2OUT |= BIT6;
				}
			}
		}
	}

	packetContainer->dataIn.preamble = 0;
	packetContainer->incomplete = 0;  // mark packet as complete (to process)

	if (packetContainer->setSignalMaster) {
		packetContainer->signalMaster = 1;
		packetContainer->setSignalMaster = 0;
	}

	msp430ResetUSCI();

	if (packetContainer->synced && packetContainer->signalMaster) {
			while (IFG2 & UCA0TXIFG)  { P2OUT ^= BIT7; __delay_cycles(50); } 
			UCA0TXBUF = 0x80 | nodeId;
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

	if (adceId & 0x01) {
		P1IES = ~BIT3;
 		if(state) {
			P1IE  |= BIT3;
			P1IFG = P1IN & BIT3;
 		} else {
			P1IE  &= ~BIT3;
			P1IFG &= ~BIT3;
	 	}
	}

	if (adceId & 0x02) {
		P2IES = ~BIT4;
		if(state) {
			P2IE  |= BIT4;
			P2IFG = P2IN & BIT4;
		} else {
			P2IE  &= ~BIT4;
			P2IFG &= ~BIT4;
		}
	}


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
	P2OUT |= adceId;
	return adceId;
}


int adceSignalTriggerAny() {

	int adceId = 0;
	if (P1IN & BIT3) adceId = 1;
	if (P2IN & BIT4) adceId = 2;

	P2OUT |= adceId;

	return adceId;
}


void adceServiceTrigger(struct PacketContainer * p, int adceId) {

	if (!adceId) return;

	p->outBufferChecksum = adceService(NULL,(unsigned char*) p->outBuffer,adceId);
	
	int trigger = p->outBuffer[8];

	if (trigger) {
		if (!p->incomplete) {
			if (p->synced) {
				UCA0TXBUF = 0x80 | nodeId;
			}
	    	p->signalMaster = 1;
	    } else {
		 	p->setSignalMaster = 1;
	    }
	}

}


int adceService(unsigned char * adceIn, unsigned char * adceOut, int adceId) {

	__delay_cycles(1500);

	 int sum = 0;
	 int i,j = 0;
	
	
	 transfer(0xAC);
	 transfer(0xAC);

	 if (adceIn) { 	 
	 	 transfer(adceIn[j++]);
		 transfer(adceIn[j++]);

		 for (i=0;i<19;i++) {
		 	*adceOut = transfer(adceIn[j++]);
		 	sum += *adceOut;
		 	adceOut++;
		 }
	 } else {
	 	 transfer(0);
		 transfer(0);

		 for (i=0;i<19;i++) {
		 	*adceOut = transfer(0);
		 	sum += *adceOut;
		 	adceOut++;
		 }
 	 }

	//P2OUT &= ~BIT0;
	//P2OUT &= ~BIT1;
	P2OUT &= ~adceId;

	__delay_cycles(500);

 	transfer(0);
 	*adceOut = adceId;
	sum += *adceOut;

	return sum;

}



void adceServiceCmd(struct PacketContainer * p, int adceId) {


	p->outBufferChecksum = adceService((unsigned char*) p->inBuffer,(unsigned char*) p->outBuffer,adceId);
	int trigger = p->outBuffer[8];

	if (trigger) {
		if (!p->incomplete) {
			if (p->synced) {
				UCA0TXBUF = 0x80 | nodeId;
			}
	    	p->signalMaster = 1;
	    } else {
		 	p->setSignalMaster = 1;
	    }
	}

}


// Initialize related stuff.

/**
 * function wordializeUSCI()
 * UART A (main comm channel with MASTER)
 *    (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 */

void initializeUSCI() {

#ifdef MSP
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
		IE2 |= UCA0RXIE;                                                // Enable USCI0 RX worderrupt
#endif

}






void initGlobal() {

#ifdef MSP
		WDTCTL = WDTPW + WDTHOLD;                               // Stop watchdog timer
		BCSCTL1 = CALBC1_12MHZ;
		DCOCTL = CALDCO_12MHZ;
		BCSCTL3 |= LFXT1S_2;                                    // Set clock source to VLO (low power osc for timer)

		P1REN &= 0;
		P1OUT &= 0;
		P1DIR = 0xFF;

		P2REN &= 0;
		P2SEL2 &= 0;
		P2SEL &= 0;

		P2DIR |= BIT6 + BIT7 + BIT0 + BIT1;                                   // Debug LEDs
		
		P1DIR |= MOSI + SCK;
		P1DIR &= ~MISO;
		P1DIR &= ~BIT3; // incoming ADCE
		P2DIR &= ~BIT4; // incoming ADCE
		
		P2IE = 0;
		P1IE = 0;

		P2OUT = 0; // bit0 outgoing  // bit1 outgoing

#endif
}


word transfer(word s) {
#ifdef MSP
    word ret=0;
    word i;

    for(i=0;i<8;i++) {


        ret <<= 1;
        // Put bits on the line, most significant bit first.
        if(s & 0x80) {
              P1OUT |= MOSI;
        } else {
              P1OUT &= ~MOSI;
        }
        P1OUT |= SCK;
        __delay_cycles( 250 );

        s <<= 1;

        // Pulse the clock low and wait to send the bit.  According to
         // the data sheet, data is transferred on the rising edge.
        P1OUT &= ~SCK;
         __delay_cycles( 250 );
        // Send the clock back high and wait to set the next bit.  
        if (P1IN & MISO) {
          ret |= 0x01;
        } else {
          ret &= 0xFE;
        }
    }
    return ret; 
#endif
}