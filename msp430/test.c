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

#define nodeId 					3
#define MI_RESCUE				0xacac 

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

		word                                         pointer;
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
void 			setADCETrigger(word state);
void 			serviceADCE();
word 			transfer(word s);
void 			clearMasterInquiry (struct PacketContainer * packetContainer);

struct 			PacketContainer * getPacketContainer(struct PacketContainer * set);

#ifndef MSP

int rxInt (word UCA0RXBUF);

#endif

word test = BIT1;

word main() {

    DISABLE_WATCHDOG();

	initGlobal();
	initializeUSCI();
	inspectAndIncrement(0,NULL); // initialize static container.



	struct PacketContainer * p = getPacketContainer(NULL);

/*
	unsigned char stream[] = {0xac, 0xac, 0xa, 0x22, 
	0x3, 0x3, 0x0, 0x0, 0x55, 0x3, 0x4, 0xff, 0x0, 0x0, 0xff, 0x0, 0x0, 
	0xff, 0x0, 0x0, 0x0, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
	0x55, 0x22, 0x39, 0x4 }; 

	p->signalMaster = 1;
	p->outBuffer[0] = 0x4433;
	p->outBuffer[9] = 0x7788;
	p->outBufferChecksum = 0x2255;

	int i;
	for (i=0;i<sizeof(stream);i++) {
		printf("In %x - Out %x\n", stream[i], rxInt(stream[i]));
	}


		for (i=0;i<sizeof(stream);i++) {
	
			if (i==7) clearMasterInquiry(NULL);
			//if (i==8) 
		printf("In %x - Out %x\n", stream[i], rxInt(stream[i]));
	}
		for (i=0;i<sizeof(stream);i++) {
		printf("In %x - Out %x\n", stream[i], rxInt(stream[i]));
	}

		return 0;
*/

	while(1) {

		setADCETrigger(1); 
		SWITCH_LOW_POWER_MODE;  
		ENABLE_INTERRUPT;

			// few things are certain at this point
			// signalMaster is 0
		    // micmd is pending, and/or acde trigger is pendinng
			// in any case adce needs to be talked with so let's do.
		P2OUT |= BIT0;

			// wait for adce callback (if not alredy there)
		while (!(P1IN & BIT3)) {
			setADCETrigger(1); // enable adce trigger (or micmd)
			SWITCH_LOW_POWER_MODE;
		}  

			// service adce and fill up next signalmaster if needed
		serviceADCE();
		DISABLE_INTERRUPT;
			// clear micmd and make space for next one
		clearMasterInquiry(p);
	}
	
	
	return 0;
}

#ifdef MSP
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {
	static word tx;
	word clearLP;
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

#else

int rxInt (word UCA0RXBUF) {
	
	static word tx;
	word clearLP;

	int txRet;
	txRet = tx;
	tx = inspectAndIncrement(UCA0RXBUF,&clearLP);
	if (clearLP) { printf("Clearing LP, Resuming.\n"); };
	
	return txRet;
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

/*
void debugPacket(const char * c,const struct PacketContainer * p) {
	printf("%s - pointer: %d, synced: %d, incomplete : %d\n",c,p->pointer,p->synced,p->incomplete);
}
*/

void            endOfReservedEvent        (const register word rx, struct PacketContainer * packetContainer)
{

		 debugPacket("endOfReservedEvent",packetContainer);
		 packetContainer->dataOut.chkSum = 0;                        // Reset Checksum
}


/**
 * function endOfHeaderEvent()
 */
void endOfHeaderEvent           (const register word rx,  struct  PacketContainer * packetContainer) {

	debugPacket("endOfHeader",packetContainer);
	if ((0xFFFF & (*(word *)&packetContainer->dataIn)) != MI_RESCUE) { 
		packetContainer->synced = 0; 
		return; 
	}  
	packetContainer->dataOut.signalMask1 =  (packetContainer->signalMaster << nodeId);
	//packetContainer->dataOut.cmd = (packetContainer->signalMaster << nodeId);

}

/**
 * function endOfHeaderEvent()
 */
void endOfDestinationEvent      (const register word rx,  struct  PacketContainer * packetContainer) {


	debugPacket("endOfDestinationEvent",packetContainer);

	if (MASTER_REQUEST_SNCC(packetContainer))    {
		packetContainer->pOut = (unsigned char*)packetContainer->outBuffer;                                     
		// Stream out buffer as of now.
		packetContainer->dataOut.snccCheckSum = packetContainer->outBufferChecksum;
	} else {
		packetContainer->dataOut.snccCheckSum = 0x00;
	}
	packetContainer->startMICMDCheckSum = MASTER_SENDING_MICMD(packetContainer);
	//printf("Master Sending CMD : %d, dest: %d \n",packetContainer->startMICMDCheckSum,packetContainer->dataIn.destinationCmd);

}

void packetPreCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer) {

	packetContainer->pOut = (unsigned char*)&packetContainer->dataOut.snccCheckSum   ; // Reset floating pointer to dataOut


}


/**
 * function endOfHeaderEvent()
 */

// micmd last byte, here we can have checksum calculated

void packetCheckSumEvent        (const register word rx,  struct  PacketContainer * packetContainer) {


	debugPacket("packetCheckSumEvent",packetContainer);
	if (packetContainer->startMICMDCheckSum) DO_CHECKSUM(packetContainer,rx); // append latest byte to chksum if needed
	//printf("CHKSUM:%x\n",packetContainer->dataOut.chkSum);
	packetContainer->startMICMDCheckSum = 0;


}

/**
 * function endOfHeaderEvent()
 // sent just after SNCC byte
 */

void endOfPacketEvent           (const register word rx, struct  PacketContainer * packetContainer) {


	debugPacket("endOfPacketEvent",packetContainer);
	if (MASTER_REQUEST_SNCC(packetContainer)) {
		if (packetContainer->dataIn.snccCheckSum == packetContainer->outBufferChecksum ) {
			packetContainer->signalMaster = 0;
			printf("SNCC SUCCESS!\n");
		}
	}

	  
}

/**
 * function endOfHeaderEvent()
 */

void endOfPacketEvent2           (const register word rx, struct  PacketContainer * packetContainer) {


	debugPacket("endOfPacketEvent2",packetContainer);
	
	if (packetContainer->clearMasterInquiry) {
		packetContainer->clearMasterInquiry = 0;
		packetContainer->masterInquiryCommand = 0;
	} else {
		if (MASTER_SENDING_MICMD(packetContainer)) {
			if (packetContainer->dataIn.chkSum == packetContainer->dataOut.chkSum) {
				packetContainer->masterInquiryCommand = 1;
				if (!packetContainer->signalMaster) {
				*packetContainer->pClearLP = 1;  /// clear low power flags on intr exit, except if there is a pending signal master, in the case don't wake up
				}
			}
		}
	}

	packetContainer->dataIn.preamble = 0;
	packetContainer->incomplete = 0;  // mark packet as complete (to process)

	P2OUT ^= BIT7;
  

	if (packetContainer->setSignalMaster) {
		packetContainer->signalMaster = 1;
		packetContainer->setSignalMaster = 0;
	}

	if (packetContainer->synced && packetContainer->signalMaster) {
			while (IFG2 & UCA0TXIFG);
			UCA0TXBUF = 0x80 | nodeId;
	}

	  
}

/**
 * function noActionEvent()
 *
 * dummy event inspector
 */
void noActionEvent                      (const register word rx, const struct  PacketContainer * packetContainer) {
	
	debugPacket("noActionEvent",packetContainer);
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

	//printf("rescue :: %x VS %x\n",0xFFFF & (word)*(word *)packetContainer->pIn,MI_RESCUE);

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

void setADCETrigger(word state) {

#ifdef MSP
	P1IES = ~BIT3;
 	if(state) {
		P1IE  |= BIT3;
		P1IFG = P1IN & BIT3;
 	} else {
		P1IE  &= ~BIT3;
		P1IFG = 0;
 	}
#endif

}

void serviceADCE() {

#ifdef MSP
	//P2OUT |= BIT7;
	__delay_cycles(10000);

	struct PacketContainer * packetContainer = getPacketContainer(NULL);
	
	int sum = 0x00;
	int i;
	 //packetContainer->signalMaster = 0;
	 unsigned char * p = (unsigned char*) packetContainer->outBuffer;

	 unsigned char stream[] = {0xAC,0xAC,0x66,0,0x00,0x00,0,0x00,0x00,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	 stream[3] = test;
	 stream[6] = test;

	 for (i=0;i<4;i++) {
	 	transfer(stream[i]);
	 }

	 for (i=4;i<sizeof(stream)-1;i++) {
	 	*p = transfer(stream[i]);
	 	sum += *p;
	 	p++;
	 }
	 
	P2OUT &= ~BIT0;
	__delay_cycles(1000);

	*p = transfer(stream[sizeof(stream)-1]);
    sum += *p;

	int trigger = packetContainer->outBuffer[8];
	packetContainer->outBufferChecksum = sum; 	

	if (trigger) {

		if (!packetContainer->incomplete) {
			if (packetContainer->synced) {
				UCA0TXBUF = 0x80 | nodeId;
			}
	    	packetContainer->signalMaster = 1;
	    } else {
		 	packetContainer->setSignalMaster = 1;
	    }
	}


	//P2OUT |= BIT7;
#endif
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
		BCSCTL1 = CALBC1_8MHZ;
		DCOCTL = CALDCO_8MHZ;
		BCSCTL3 |= LFXT1S_2;                                    // Set clock source to VLO (low power osc for timer)

		P1REN &= 0;
		P1OUT &= 0;
		P1DIR = 0xFF;

		P2REN &= 0;
		P2SEL2 &= 0;
		P2SEL &= 0;

		P2DIR |= BIT6 + BIT7 + BIT0;                                   // Debug LEDs
		
		P1DIR |= MOSI + SCK;
		P1DIR &= ~MISO;
		P1DIR &= ~BIT3; // incoming


		P2OUT = 0; // bit0 outgoing

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