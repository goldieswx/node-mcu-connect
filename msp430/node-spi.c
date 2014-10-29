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

#define node2_0 1
#define ADCE 1

// global declarations

int action;
#define currentNodeId   3
  // id of this node

#define PROCESS_BUFFER 0x01
#define ADC_CHECK      0x02


#define MI_CMD      0x220a //    8714

#define word int
// MCOM related declarations

#define MI_PREAMBLE  0xac // 0b10101100

#define MI_RESCUE   0x220aacac

#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

typedef struct _McomInPacket {
  unsigned word preamble;
  unsigned word cmd;
  unsigned char destinationCmd;
  unsigned char destinationSncc;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[20];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomInPacket;

typedef struct _McomOutPacket {
  unsigned word preamble;
  unsigned word cmd;
  unsigned char signalMask2;
  unsigned char signalMask1;
  unsigned char __reserved_1;
  unsigned char __reserved_2;
  unsigned char data[20];
  unsigned word snccCheckSum;
  unsigned word chkSum;
} McomOutPacket;


#define packetLen (sizeof(McomInPacket))

McomInPacket 	 inPacket;
McomOutPacket   outPacket;

unsigned char * pInPacket;
unsigned char * pOutPacket;

unsigned char  outBuffer[packetLen];
int            outBufferCheckSum;
int            signalMaster; 
int            mcomPacketSync;


unsigned char * pckBndPacketEnd;
unsigned char * pckBndHeaderEnd;
unsigned char * pckBndDestEnd;
unsigned char * pckBndDataEnd;

int            transmissionErrors;
int            checkSum; // checksum used in the SPI interrupt
int            preserveInBuffer; // preserve input buffer (data in InPacket) when
                                // processing buffer and while communicating (after PB acion was set).

void initGlobal();
void initMCOM();
void mcomProcessBuffer();
void zeroMem(void * p,int len);
void initDebug();

//// extension related
void initADCE();
void checkADC(); 
char transfer(char s); 
void ioMSG();

int main() {

	initGlobal();
	initMCOM();
  initDebug();
#ifdef ADCE
  initADCE();
#endif

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
      if (action & ADC_CHECK) {
          checkADC(); 
          continue;
      }
	}

}

/**
 * MCOM Related
 */
void mcomProcessBuffer() {

//     __disable_interrupt();
     __enable_interrupt();

     ioMSG();
     action &= ~PROCESS_BUFFER; 
     // when we release the action, we will stop being busy and
     // contents of inBuffer must be ready to reuse
     // (and will be destroyed)
  

     return;

}


void _signalMaster() {
	
 /* outBuffer[0] = 0xab;
  outBuffer[1] = 0xcd;
  outBuffer[2] = 0xef;
  outBuffer[3] = 0x99;
  outBuffer[19] = 0xff;
  outBuffer[20] = 0xff; 
  outBufferCheckSum = 0x3ff; */

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


void zeroMem(void * p,int len) {
     
     while (len--) {
        *(unsigned char*)p++ = 0x00;
     }
}


void initMCOM() {

  // Software related stuff

  transmissionErrors = 0;
  mcomPacketSync = 0;
  zeroMem(&inPacket,packetLen);
  zeroMem(&outPacket,packetLen);
  zeroMem(outBuffer,packetLen);
  signalMaster = 0;

  pInPacket = (unsigned char *)&inPacket;
  pOutPacket = (unsigned char*)&outPacket;
  pOutPacket++;

  pckBndPacketEnd = (unsigned char*)&inPacket;
  pckBndPacketEnd += packetLen;
  pckBndPacketEnd--;

  pckBndHeaderEnd = &(inPacket.destinationCmd);
  //pckBndHeaderEnd--;

  pckBndDestEnd   = &(inPacket.__reserved_1); // sent 1B before end, txbuffer is always 1b late.
  pckBndDataEnd   = (unsigned char*)&(inPacket.snccCheckSum);
  pckBndDataEnd--;

  preserveInBuffer = 0;
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


void initDebug() {

#ifdef node2_0
  P2OUT &= ~(BIT6 + BIT7);
  P2SEL2 &= ~(BIT6 + BIT7);
  P2SEL &= ~(BIT6 + BIT7);
  P2DIR |= BIT6 + BIT7;
#else
  P1DIR |= BIT0 + BIT3;
#endif

}

void initGlobal() {

  WDTCTL = WDTPW + WDTHOLD;   // Stop watchdog timer
  BCSCTL1 = CALBC1_12MHZ;
  DCOCTL = CALDCO_12MHZ;
  BCSCTL3 |= LFXT1S_2;                      // Set clock source to VLO (low power osc for timer)

  P1REN &= 0; 
  P1OUT &= 0;
  P2REN &= 0;
  P2OUT &= 0;

  P2SEL2 &= 0;
  P2SEL &= 0;

#ifdef node2_0  
  P2DIR |= BIT6 + BIT7;  // debug;
 // P2OUT |= BIT7;
#else
  P1DIR |= BIT0 + BIT3;  // debug;
  P1OUT |= BIT3;
#endif
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

   if (UCA0STAT & UCOE) {
	// buffer overrun occured
	mcomPacketSync = 0;
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

                    #ifdef node2_0  
                      // P2OUT ^= BIT6;
                    #else
                      P1OUT ^= BIT0;
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
                      P2OUT &= inPacket.data[0];
                      P2OUT |= inPacket.data[1];
                      //action |= ADC_CHECK;    
                      //_signalMaster();
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
             if (action & PROCESS_BUFFER) {  // we are busy, preserve the input buffer, we
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
                outPacket.chkSum = 0;
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


/// Extension related stuff


#define MOSI  BIT7 
#define MISO  BIT6
#define SCK   BIT5

#define CS_NOTIFY_MASTER  BIT3   // External Interrupt 
#define CS_INCOMING_PACKET  BIT0   // Master enable the line before sending

#define _CNM_PIE          P1IE
#define _CNM_PIFG         P1IFG
#define _CNM_PDIR         P1DIR
#define _CNM_PIES         P1IES
#define _CNM_PREN         P1REN
#define _CNM_PORT_VECTOR  PORT1_VECTOR

#define _CIP_POUT         P2OUT
#define _CIP_PDIR         P2DIR

#define ADC_CHECK      0x02

interrupt(_CNM_PORT_VECTOR) p2_isr(void) { //PORT2_VECTOR

  //__enable_interrupt();

  if (_CNM_PIFG & CS_NOTIFY_MASTER) {
    _CNM_PIE &= ~CS_NOTIFY_MASTER;
    action |= ADC_CHECK;
    __bic_SR_register_on_exit(LPM3_bits /*+ GIE*/); // exit LPM     
  }

  return;
  
} 


/**
 *  ADCE Related 
 */

void initADCE() {
    
  _CNM_PDIR &= ~CS_NOTIFY_MASTER ;
  _CNM_PIE |=  CS_NOTIFY_MASTER ; 
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

void ioMSG() {
  
    _CIP_POUT |= CS_INCOMING_PACKET;    // Warn ADCE that we are about to start an spi transfer.
    __delay_cycles(10000);
    
    // delayCyclesProcessBuffer(20); // Give some time to ADCE to react
    
                    //P2OUT &= (inPacket.data[0] & (BIT6 | BIT7));
                    //P2OUT |= inPacket.data[1];


    //*p = transfer(inPacket.data[0]);               // Get Packet length from ADCE
    //*p = transfer(inPacket.data[0]);               // Get Packet length from ADCE
    //*p = transfer(inPacket.data[0]);               // Get Packet length from ADCE

    transfer(1);

    int i = 0;
    for(i=0;i<10;i++) {
       __delay_cycles(1000);
       transfer(inPacket.data[i]);
    }

    __delay_cycles(1000);
    _CIP_POUT &= ~CS_INCOMING_PACKET;   // release extension signal
    __delay_cycles(10000);

    //outBuffer[0] = inPacket.data[0];
    //outBuffer[1] = inPacket.data[1];

    //_signalMaster();

    /*int len = 2;

    while (len--) {
        *p++ = transfer(0);
        __delay_cycles(10000);
       // P2OUT ^= BIT7;
    }*/   //__delay_cycles(100000);



}


void checkADC() {
    

    _CNM_PIE &= ~CS_NOTIFY_MASTER;
    _CNM_PIFG |= CS_NOTIFY_MASTER;    // Just preacaution, we will soon enable interrupts, make sure we don't allow reenty.
    __enable_interrupt();         // Our process is low priority, only listenning to master spi is the priority.
    
    action &= ~ADC_CHECK;         // Clear current action flag.
    
   // P2OUT ^= BIT6;  // debug        // ADC Extension (ADCE) is a module ocnnected thru USCI-B and two GPIO pins
  
    _CIP_POUT |= CS_INCOMING_PACKET;    // Warn ADCE that we are about to start an spi transfer.
    
    // delayCyclesProcessBuffer(20); // Give some time to ADCE to react
    __delay_cycles(10000);


    unsigned char * p = outBuffer;
    
    transfer(2);               // Get Packet length from ADCE
    
    int len = 16 ;


    __delay_cycles(10000);

    while (len--) {
        *p++ = transfer(0);
        __delay_cycles(1000);

       // P2OUT ^= BIT7;
    }

    //lastresp[16] = c[0];            // Temporarily set the response somewhere (FIX)
    //lastresp[17] = c[1];
    //lastresp[18] = c[2];
    //lastresp[19] = 0x07;

    // action |= SIGNAL_MASTER;        // Inform master we have some data to transmit.
    __delay_cycles(1000);
 
    _signalMaster();

    _CNM_PIFG &= ~CS_NOTIFY_MASTER;        
    _CNM_PIE |= CS_NOTIFY_MASTER;
    _CIP_POUT &= ~CS_INCOMING_PACKET;   // release extension signal

     __enable_interrupt();   
    __delay_cycles(1000); // give some time to treat the incoming packet update


}

char transfer(char s) {

    unsigned char ret=0;
    int i;

    for(i=0;i<8;i++) {
        P1OUT |= SCK;
        __delay_cycles( 500 );

        ret <<= 1;
        // Put bits on the line, most significant bit first.
        if(s & 0x80) {
              P1OUT |= MOSI;
        } else {
              P1OUT &= ~MOSI;
        }
        s <<= 1;

        // Pulse the clock low and wait to send the bit.  According to
         // the data sheet, data is transferred on the rising edge.
        P1OUT &= ~SCK;
        __delay_cycles( 500 );

        // Send the clock back high and wait to set the next bit.  
        if (P1IN & MISO) {
          ret |= 0x01;
        } else {
          ret &= 0xFE;
        }
    }
    return ret; 

}
