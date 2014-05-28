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

#define PROCESS_BUFFER 0x01

// MCOM related declarations


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
  unsigned word chkSum
} _McomPacket;

#define packetLen (sizeof(mcomPacket))

McomPacket pcks[2];

McomPacket *     pckCurrentTransferred;  // current packet being tranferred
void *     pckCurTransferCursor;    // cursor (*p++ like) of current packet being trsf.
void *     pckBndTransferCursor;    // boundary of current packet being trsf. (pointer to last current byte + 1) 
McomPacket *     pckCurrentProcess;      // current packet being processed

char       pckRescue[4];
char  *    pckRescueCursor;
char  *    pckRescueBoundary;
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
	}

}

/**
 * MCOM Related
 */
void mcomProcessBuffer() {



}


void initMCOM() {

  // Software related stuff

  pckCurrentTransferred = pck;
  pckCurrentProcess = pck+1;
  pckCurTransferCursor = pckCurrentTransferred;
  pckBndTransferCursor = pckCurrentProcess;

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


// INTERRUPTS
interrupt(USCIAB0RX_VECTOR) USCI0RX_ISR(void) {

	if (mcomSynced) {
	    (*pckCurTransferCursor++) = UCA0RXBUF;
	    UCA0TXBUF = (*pckCurTransferCursor);

	    if (pckCurTransferCursor == pckBndTransferCursor) {
	      action |= PROCESS_BUFFER;
	      __bic_SR_register_on_exit(LPM3_bits); // exit LPM, keep global interrupts state      
	    }
	} else {
		UCA0TXBUF = 0x00;
		// scan stream and try to find a preamble start sequence
		// (Oxac Oxac KNOWN CMD)
		// we have in the int and have very few cycles to intervene 
        *rescue++ = UCA0RXBUF;
        if (boundary){
        	if rescue == 0xacac micmd
            i		
        }
	}

    return;
}
