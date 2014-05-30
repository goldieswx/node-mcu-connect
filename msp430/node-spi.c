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

#define MI_CMD      0x220a //    8714


// MCOM related declarations

#define MI_PREAMBLE  0xac // 0b10101100

#define MI_RESCUE   0xacac220a

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

typedef struct McomPacket {
  unsigned word preamble,
  unsigned word cmd,
  unsigned char destinationCmd,
  unsigned char destinationSncc,
  unsigned char __reserved_1,
  unsigned char __reserved_2,
  unsigned char data[20],
  unsigned word snccCheckSum,
  unsigned word chkSum
} _McomPacket;


#define packetLen (sizeof(mcomPacket))

McomPacket 	 inPacket;
McomPacket   outPacket;

unsigned char * pInPacket;
unsigned char * pOutPacket;

unsigned char  outBuffer[packetLen];
int            outBufferCheckSum;
int            signalMaster; 


unsigned char * pckBndPacketEnd;
unsigned char * pckBndHeaderEnd;
unsigned char * pckBndDestEnd;
unsigned char * pckBndDataEnd;

int            preserveInBuffer; // preserve input buffer (data in InPacket) when
                                // processing buffer and while communicating (after PB acion was set).

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
	}

}

/**
 * MCOM Related
 */
void mcomProcessBuffer() {

     __disable_interrupt();
    
       action &= ~PROCESS_BUFFER; // when we release the action, 
       // contents of inBuffer must be ready to reuse
       // (and will be destroyed)
     __enable_interrupt();
     return;

}


void initMCOM() {

  // Software related stuff

  mcomPacketSync = 0;
  zeroMem(&inPacket,packetLen);
  zeroMem(&outPacket,packetLen);
  zeroMem(outBuffer,packetLen)
  signalMaster = 0;

  pInPacket = (unsigned char *)&inPacket;
  pOutPacket = (unsigned char*)&outPacket;

  pckBndPacketEnd = inPacket;
  pckBndPacketEnd += packetLen;
  pckBndHeaderEnd = &(inPacket.destinationCmd);
  pckBndDestEnd   = &(inPacket.__reserved_2); // sent 1B before end, txbuffer is always 1b late.
  pckBndDataEnd   = &(inPacket.snccCheckSum);

  // Hardware related stuff
 
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

  unsigned char savepInPacket = (*pInPacket); 
  (*pInPacket) = UCA0RXBUF;
 
	if (mcomSynced) {
      UCA0TXBUF = (*pOutPacket++);

	    switch (pInPacket) {
	      case pckBndPacketEnd:
          if (inPacket.destinationSncc == currentNodeId) {  // there was a sncc transfer
              if (inPacket.reserved == outBufferCheckSum) { // successful
                  outPacket.signalMask1 = 0: // clear signal mask meaning master received the sncc buffer.
              }
          }
          pInPacket = &inPacket;
          pOutPacket = &outPacket;
		      if (inPacket.destinationCmd == currentNodeId) { // there was a mi cmd
            if (inPacket.chkSum == outPacket.chkSum) {
               action |= PROCESS_BUFFER;
                __bic_SR_register_on_exit(LPM3_bits); // exit LPM, keep global interrupts state      
            }
          }
          break;
	      case pckBndHeaderEnd:
            if ((*(long *)&inPacket) == MI_RESCUE) {
               outPacket.signalMask1 |= (signalMaster >> currentNodeId);
               signalMaster = 0;
            } else {
                mcomSynced--;
                preserveInBuffer = 0;
            }
          break;
        case pckBndDestEnd:
           if (inPacket.destinationSncc == currentNodeId) && (outPacket.signalMask1) {
               // if master choose us as sncc and if we signalled master
              pOutPacket = outBuffer; // outBuffer contains sncc data payload
           }
           checkSum = 0; // start checksumming
           if (action & PROCESS_BUFFER) {  // we are busy, preserve the input buffer, we
                                           // are processing it
              *pInPacket = 0; // normally this is a reserved byte
                              // force to 0 just so checksum will be correct in any case
              preserveInBuffer = 1;
           }
           break;
         case pckBndDataEnd:
             // exchange chksums
             preserveInBuffer = 0;
             pOutPacket = &(outPacket.chkSum);
             pOutPacket--;

             if ((inPacket.destinationSncc == currentNodeId)  // it's our turn (sncc)
                 && (outPacket.signalMask1))                 //  we signalled master
             {
                outPacket.snccCheckSum = outBufferCheckSum;
             } else {
                outPacket.snccCheckSum = 0;
             } 

             if (inPacket.destinationCmd == currentNodeId) // also send cmd chk (lowest Byte) if needed
             { 
                outPacket.chkSum = checkSum;
             } else {
                outPacket.chkSum = 0;
             }
	    }
      checkSum += (*pInPacket);
      if (preserveInBuffer) { (*pInPacket) = savepInPacket; }
      pInPacket++;

	} else {
		UCA0TXBUF = 0x00;
		// scan stream and try to find a preamble start sequence
		// (Oxac Oxac KNOWN CMD)
		// we are in the intr and have very few cycles to intervene 
        pInPacket = &InPacket;
        unsigned char* rescuePtrSrc = pInPacket;
        rescuePtrSrc++;

        *pInPacket++ = *rescuePtrSrc++;
        *pInPacket++ = *rescuePtrSrc++;
        *pInPacket++ = *rescuePtrSrc;
        *pInPacket++ = UCA0RXBUF;
      
      	if ((*(long *)&InPacket) == MI_RESCUE) {
        	mcomSynced++;
        }
	}
  return;
}
