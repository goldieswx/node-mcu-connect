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


// global declarations

int action;
#define currentNodeId             8  // id of this node

#define PROCESS_BUFFER 0x01
#define TREAT_PACKET   0x02

#define MI_CMD      0x220a //    8714
#define SNCC_CMD    0x30a5  // Mi issues an explicit SNCC 

// MCOM related declarations

#define MI_PREAMBLE  0xac // 0b10101100

#define MI_RESCUE1   0xacac220a
#define MI_RESCUE2   0xacac30a5

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

typedef struct McomPacket
{ unsigned word preamble,
  unsigned word cmdId,
  unsigned word from,
  unsigned word mask,
  unsigned char data[20],
  unsigned word reserved,
  unsigned word chkSum,
  unsigned word _metaActionProcessed
} _McomPacket;


#define packetLen (sizeof(mcomPacket))

McomPacket 	   pcks[2];


McomPacket *     pckCurrentTransferred;  // current packet being tranferred
char *     pckCurTransferCursor;    // cursor (*p++ like) of current packet being trsf.
char *     pckBndTransferCursor;    // boundary of current packet being trsf. (pointer to last current byte + 1) 
McomPacket *     pckCurrentProcess;      // current packet being processed

unsigned char mcomProcessingBuffer = 0;
unsigned int  transferErrors= 0;

////////// 


void main() {

	initGlobal();
	initMCOM();

	while(1) {
	     __enable_interrupt();
	    if (action == 0) {
		    __bis_SR_register(LPM3_bits + GIE);   // Enter LPM3, enable interrupts // we need ACLK for timeout.
	    }
	    __disable_interrupt(); // needed since action must be locked while being compared
	  	if (action & PROCESS_BUFFER) {
	        mcomProcessBuffer(); 
	        continue;
	    }
	    if (action & TREAT_PACKET) {
	    	mcomTreatPacket();
	    }
	}

}

/**
 * MCOM Related
 */

void mcomTreatPacket() {

	packet->_metaActionProcessed = 0;

}


void mcomProcessBuffer() {

     __disable_interrupt();
     // pckCurTransferCursor has just been filled.
     // switching buffers.
     McomPacket * packet = pckCurrentTransferred;
     pckCurrentTransferred = pckCurrentProcess;
     pckCurrentProcess = packet;
    
     // boundary of packet (physical transfer) is at start of metadata
     pckBndTransferCursor = (char*) &(pckCurrentTransferred._metaActionProcessed);
     
     action &= ~PROCESS_BUFFER;
     mcomProcessingBuffer ++;
     __enable_interrupt();

     // check packet validity.

     if ((packet->preamble == 0xacac) &&
    	 ((packet->cmdId == MI_CMD) || (packet->cmdId == SNCC_CMD))) {

     		// verify data checksum
     		int chkSum = 0, dataLen = sizeOf(McomPacket.data);
     		while (dataLen) {
     			chkSum += packet->data[dataLen];
     		}

     		if (packet->chkSum != chkSum) {
			packet->_metaErr |= ERR_CHECKSUM;
     			// error wrong packet
     		} else {
	            // packet is for us.
	     		if (packet->from == currentNodeId) {
	                 action |= TREAT_PACKET; 
	                 packet->_metaActionProcessed = 0;
	     		}
	     	}
     } else {
     	if (transferErrors++ > 2) {
     		mcomSynced = 0;
     	}
	 // error wrong packet sets rescue mode
        
     }

     mcomProcessingBuffer--;
     return;

}


void initMCOM() {

  // Software related stuff

  pckCurrentTransferred = pck;
  pckCurrentProcess = pck+1;
  pckCurTransferCursor = pckCurrentTransferred;
  // boundary of packet (physical transfer) is at start of metadata
  pckBndTransferCursor = (char*) &(pckCurrentTransferred._metaActionProcessed);
  
  mcomSynced = 0;

  // Hardware related stuff
 
  // UART A (main comm chanell with MASTER)
  //     (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 
  P1DIR |= BIT1;
  P1DIR &= ~(BIT2 | BIT4);
  P1SEL &=  ~(BIT1 | BIT2 | BIT4);
  P1SEL2 &=  ~(BIT1 | BIT2 | BIT4);

  resyncToStream(); 

  P1SEL |=  BIT1 | BIT2 | BIT4;
  P1SEL2 |=  BIT1 | BIT2 | BIT4;
    
  UCA0CTL1 = UCSWRST;                       // **Put state machine in reset**
  __delay_cycles(10);
  UCA0CTL0 |= UCCKPH + UCMSB + UCSYNC;     // 3-pin, 8-bit SPI slave
  UCA0CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**

  while(IFG2 & UCA0RXIFG);                  // Wait ifg2 flag on rx  (no idea what it does)
  IE2 |= UCA0RXIE;                          // Enable USCI0 RX interrupt
  UCA0TXBUF = 0x00;                         // We do not want to ouput anything on the line

}

void initGlobal() {

  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer
  BCSCTL1 = CALBC1_8MHZ;
  DCOCTL = CALDCO_8MHZ;
  BCSCTL3 |= LFXT1S_2;                      // Set clock source to VLO (low power osc for timer)

  P1REN &= 0; 
  P1OUT &= 0;
  P2REN &= 0;
  P2OUT &= 0;

  P2SEL2 &= 0;
  P2SEL &= 0;
  
  P2DIR |= BIT6 + BIT7;  // debug;

}

/*Node (booting up)
 
  => (sync)
  => [bfr micmd to 2] -> b1in send 0x00s 
  => [bfr micmd to 2] -> b2in send 0x00s (process b1in)

(signal/ack)
  => [bfr micmd to 2] -> cmd will be ignored (won't happen, maybe node could send flag busy) ack b1 (or signal b1)
  => [bfr micmd to 2] -> cmd will be ignored (won't happen, maybe node could send flag busy) ack b2 , signal b1 (and or b2)
  
  => [bfr sncc to 2] ->  reponse with answer correponding to cmdid signalled
  */


// INTERRUPTS
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	if (mcomSynced) {
	    (*pckCurTransferCursor++) = UCA0RXBUF;
	    UCA0TXBUF = (*pckCurTransferCursor);

	    switch (pckCurTransferCursor)
	      case pckBndTransferCursor:
		      action |= PROCESS_BUFFER;
		      __bic_SR_register_on_exit(LPM3_bits); // exit LPM, keep global interrupts state      
		      break;
	      case pckBndHeaderEnd:
	    	//check is SNCC CMD
	    	pckCurTransferCursor  = ptrToData;
	    	pckBndTransferCursor  = ptrToData;
	    	break;
              case _signal:
	    	apply nextSignal to bfr; clear nextsignal
	    	break;
	      case _ack:
	    	apply ack to bfr clear ack
	    	break;
	    }
	} else {
		UCA0TXBUF = 0x00;
		// scan stream and try to find a preamble start sequence
		// (Oxac Oxac KNOWN CMD)
		// we are in the intr and have very few cycles to intervene 
        unsigned char* rescuePtr = pckCurrentTransferred;
        *rescuePtr = *rescuePtr++;
        *rescuePtr = *rescuePtr++;
        *rescuePtr = *rescuePtr++;
        *rescuePtr = UCA0RXBUF;
      
      	switch (*(long *)pckRescue) {
      	case MI_RESCUE1:
      	case MI_RESCUE2:
        	mcomSynced++;
        	pckCurTransferCursor = (*++rescuePtr);
        }
	}

    return;
}
