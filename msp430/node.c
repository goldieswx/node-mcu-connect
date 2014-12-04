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

McomInPacket    inPacket;
McomOutPacket   outPacket;

unsigned char * pInPacket;
unsigned char * pOutPacket;

static register unsigned int tx = 0;
static register FUNC packetInspectorEvent;

static unsigned int packetPtr;
static unsigned int incompletePacket = 1;


int main() {
	
	initSPI();
	//initADCE();

	while(1) {	
		while(incompletePacket) {
			while(IFG2 & UCA0RXIFG) {
					// SPI byte received 
					UCA0TXBUF = tx;
					packetInspectorEvent(UCA0RXBUF);
					updatePacketInspectorPtr();
			}
		}
		transferPendingIO(); // transfer pending messages from/to adce if any
	}

}


inline void updatePacketInspectorPtr() {

	pInPacket++;
	pOutPacket++;
	packetPtr++;

	switch (packetPtr) {
		case 4:
			packetInspectorEvent = &endOfPreambleEvent;
		case 8:
			packetInspectorEvent = &endOfHeaderEvent;
		case 20:
			packetInspectorEvent = &packetCheckSumEvent;
		case 32:
			packetInspectorEvent = &endOfPacketEvent
		default:
			packetInspectorEvent = &noActionEvent;
	}

}


void endOfPreambleEvent		(register int rx) {
	// check validity of preamble event
	// send sncc id if pending

}

void endOfHeaderEvent		(register int rx) {

}

void packetCheckSumEvent	(register int rx) {
	// send sncc packet checksum
	// send micmd packet checksum
	// send micmd packet checksum if busy.
}

void endOfPacketEvent		(register int rx) {

	// (MICMD Successful (or no MICMD)) AND (No more sncc pending)  then incompletePacket = 0;
	// Otherwise incompletePacket = 1;
    
}

void noActionEvent			(register int rx) {
	*pInPacket = rx;
}


void transferPendingIO() {
	
	// check P1IN (adce intr) for a hi signal
	// do if(p1in or micmd pending) 
		// bring SIGNAL hi to adce
		// initiate spi transfer
		// bring SIGNAL lo to adce
		// (repeat until valid checksums)
	// incompletepacket = 1

}


/**
 * MCOM Related
 */
void mcomProcessBuffer() {

	 __enable_interrupt();

	 if (!busy) { _memcpy(inDataCopy,inPacket.data,20); }
	 busy = 1;

	 if (_CNM_PIN & CS_NOTIFY_MASTER)  {
		  // adce is currently transferring something, just wait and retry.
		  TA0CCR0 = 500;
		  TA0CTL = TASSEL_1 | MC_1 ;             // Clock source ACLK
		  TA0CCTL1 = CCIE ;                      // Timer A interrupt enable
	 } else {
		 // clear to send to adce, just go.
		  P2OUT ^= BIT7;
		_CIP_POUT |= CS_INCOMING_PACKET;  // Dear ADCe, We have some data that needs to be transferred.
		action |= ADC_CHECK;
	 }

	 action &= ~PROCESS_BUFFER;

	 // when we release the action (process_buffer), we will stop being busy and
	 // contents of inBuffer must be ready to reuse
	 // (and will be destroyed)
	return;

}





void _signalMaster() {
  

	int i;
	unsigned int chk = 0;
	for (i=0;i<20;i++) {
	  chk += outBuffer[i];
	}

	outBuffer[20] = chk & 0xFF;
	outBufferCheckSum = chk;

	__disable_interrupt();  

	signalMaster = 1;
	// in case we are synced (out of a rescue, and not in a middle of a packet, force MISO high to signal master.)
	//__disable_interrupt();
	if (mcomPacketSync && (pInPacket == (unsigned char*)&inPacket)) { // not in receive state, but synced
	  if (UCA0TXIFG) {
		UCA0TXBUF = 0x80 | currentNodeId;
	  }
	}

  
}



void initSPI() {

  // UART A (main comm chanell with MASTER)
  //     (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 
  P1DIR |= BIT1;
  P1DIR &= ~(BIT2 | BIT4);
  P1SEL &=  ~(BIT1 | BIT2 | BIT4);
  P1SEL2 &=  ~(BIT1 | BIT2 | BIT4);

  P1SEL |=  BIT1 | BIT2 | BIT4;
  P1SEL2 |=  BIT1 | BIT2 | BIT4;
	
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



// INTERRUPTS
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

  unsigned char savepInPacket = (*pInPacket); 

   if (UCA0STAT & UCOE) {
  // buffer overrun occured
	  mcomPacketSync = 0;
	  WDTCTL = WDTHOLD;
   }

  (*pInPacket) = UCA0RXBUF;
 
	  if (mcomPacketSync) {

	   UCA0TXBUF = (*pOutPacket++);

		/* case pckBndPacketEnd */
		if (pInPacket==pckBndPacketEnd) { 
			UCA0TXBUF = 0x00;
			//P2OUT |= BIT7;
			if (inPacket.destinationSncc == currentNodeId) {  // there was a sncc transfer
				if (inPacket.snccCheckSum == outBufferCheckSum) { // successful
					outPacket.signalMask1 = 0; // clear signal mask meaning master received the sncc buffer.
					
					action |= processPendingActionUponSNCC;
					processPendingActionUponSNCC = 0;
					if (action) {  __bic_SR_register_on_exit(LPM3_bits); }
					#ifdef node2_0  
					  // P2OUT ^= BIT6;
					#else
					   //UT ^= BIT0;
					  //action |= ADC_CHECK;    
					#endif
				}
			}
			pInPacket = (unsigned char*)&inPacket;
			pInPacket--; // -- since it is sytematically increased
			pOutPacket = (unsigned char*)&outPacket;
			pOutPacket++; // -- since we are sytematically 1B late

			if (signalMaster || outPacket.signalMask1) { // since we started receving the pkgs master was signalled
			  UCA0TXBUF = 0x80 | currentNodeId;
			}

			if (inPacket.destinationCmd == currentNodeId) { // there was a mi cmd
			  if (inPacket.chkSum == outPacket.chkSum) {
				   #ifdef node2_0  
					  //P2OUT &= inPacket.data[0];
					  //P2OUT |= inPacket.data[1];
					  //action |= ADC_CHECK;    
					  //_signalMaster();
					  //P2OUT ^= BIT7;
					#else
					  P1OUT ^= BIT3;
					#endif
				 action |= PROCESS_BUFFER;
				  __bic_SR_register_on_exit(LPM3_bits); // exit LPM, keep global interrupts state      
			  }
			}
			goto afterChecks;
		}

		/* case pckBndHeaderEnd */
		if (pInPacket==pckBndHeaderEnd) {
			  if ((*(long *)&inPacket) == MI_RESCUE) {
				  outPacket.signalMask1 |= (signalMaster << currentNodeId);
				  signalMaster = 0;
			  } else {
				  mcomPacketSync = 0;
				  preserveInBuffer = 0;
				  inPacket.preamble =0;
				  inPacket.cmd = 0;
			  }
			goto afterChecks;
		}

		/* case pckBndDestEnd */
		if (pInPacket==pckBndDestEnd) {

			 if ((inPacket.destinationSncc == currentNodeId) && (outPacket.signalMask1)) {
				 // if master choose us as sncc and if we signalled master
				pOutPacket = outBuffer; // outBuffer contains sncc data payload
			 }
			 checkSum = 0; // start checksumming
			 if (busy || (action & PROCESS_BUFFER)) {  // we are busy, preserve the input buffer, we
											 // are processing it
				*pInPacket = 0; // normally this is a reserved byte
								// force to 0 just so checksum will be correct in any case
				preserveInBuffer = 1;
			 }
			 goto afterChecks;
		 }
		 /* case pckBndDataEnd */
		 if  (pInPacket==pckBndDataEnd) {
			 // exchange chksums
			 pOutPacket = (unsigned char*) &(outPacket.chkSum);
			 pOutPacket--;

			 if ((inPacket.destinationSncc == currentNodeId)  // it's our turn (sncc)
				 && (outPacket.signalMask1)) {               //  we signalled master
				outPacket.snccCheckSum = outBufferCheckSum;
			 } else {
				outPacket.snccCheckSum = 0;
			 } 
			 if ((!preserveInBuffer) && (inPacket.destinationCmd == currentNodeId)) 
			 {   // also send cmd chk if it's its turn and if we are not busy 
				outPacket.chkSum = checkSum + (*pInPacket);
			 } else {
				if (inPacket.destinationCmd == currentNodeId) {
					outPacket.chkSum = checkSum + (*pInPacket)+1; // send out a bad chksum if we can transmi data on the line, otherwise say 0
				} else {
					outPacket.chkSum = 0; // just shut up (not our turn)
				}
			 }
			 preserveInBuffer = 0;
		  }
	  /* after all error checks */    afterChecks:
	  checkSum += (*pInPacket);
	  if (preserveInBuffer) { (*pInPacket) = savepInPacket; }
	  pInPacket++;
	  return;

  } else {
	  UCA0TXBUF = 0x00;
	// scan stream and try to find a preamble start sequence
	// (Oxac Oxac KNOWN CMD)
	// we are in the intr and have very few cycles to intervene 
		pInPacket = (unsigned char*)&inPacket;
		unsigned char* rescuePtrSrc = pInPacket;
		rescuePtrSrc++;

		*pInPacket++ = *rescuePtrSrc++;
		*pInPacket++ = *rescuePtrSrc++;
		*pInPacket++ = *rescuePtrSrc;
		*pInPacket++ = UCA0RXBUF;
	  
		if ((*(long *)&inPacket) == MI_RESCUE) {
			// On (first) sync:
			//Enable INTR interrupt on raising edge.

			_CNM_PIE |=  CS_NOTIFY_MASTER ; 
			_CNM_PIFG |=  _CNM_PIN & CS_NOTIFY_MASTER; // force interrupt to be called if input is already high.

		  mcomPacketSync++;
		  // sync out buffer also.
		  pOutPacket = ((unsigned char*)&outPacket)+5; // sizeof double preamble, cmd + late byte
		} else {
		  if(transmissionErrors++ >= 100) {
			WDTCTL = WDTHOLD;
		  }
		}

  }
  return;
}




interrupt(_CNM_PORT_VECTOR) p2_isr(void) { //PORT2_VECTOR

  //__enable_interrupt();

  if (_CNM_PIFG & CS_NOTIFY_MASTER) {
	 if (signalMaster||outPacket.signalMask1)  { 
		processPendingActionUponSNCC = ADC_CHECK;
	 } else {
		 action |= ADC_CHECK;
		__bic_SR_register_on_exit(LPM3_bits + GIE); // exit LPM     
	 }
  }

  _CNM_PIFG &= 0;
  return;
  
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

void checkADC() {

	__enable_interrupt();         // Our process is low priority, only listenning to master spi is the priority.


	while (!(_CNM_PIN & CS_NOTIFY_MASTER)) {
	  __delay_cycles(1000);
	}
	_CIP_POUT &= ~CS_INCOMING_PACKET;   // release extension signal

	__delay_cycles(4000);  // Give some time to ADCE to react
	__delay_cycles(4000);  // Give some time to ADCE to react
	__delay_cycles(4000);  // Give some time to ADCE to react


   // static int debug = 0;

	transfer(0xAC); // we should check preamble received
	transfer(0xAC);

	unsigned int i;
	unsigned char trsfBuf[20];
	for (i=0;i<19;i++) {
		 //outBuffer[i] = transfer(inDataCopy[i]);
		 trsfBuf[i] = transfer(inDataCopy[i]);
	}

	_CIP_POUT ^= CS_INCOMING_PACKET;   // pulse signal for last byte.
	//outBuffer[19] = transfer(inDataCopy[19]);
	trsfBuf[19] = transfer(inDataCopy[i]);
	_CIP_POUT ^= CS_INCOMING_PACKET;   // pulse signal for last byte.
  
	//outBuffer[19] = debug++;
	//debug %= 256;

	if (trsfBuf[12] && (!(signalMaster||outPacket.signalMask1))) { 

		_memcpy(outBuffer,trsfBuf,20);
		_signalMaster(); 
	} // TODO : else, there is something in the OutBuffer, and we have sent out in the return a
	  // cmd to the adce and received another OutBuffer, yet we have now 2 buffer to process, this shouldn' happen, fix, pass a param
	  // in the spi stream saying node busy or buffer this .


	busy = 0; // we're finished with buffer
	action &= ~ADC_CHECK;            // Clear current action flag.
	return;
}



