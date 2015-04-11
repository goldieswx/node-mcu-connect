#ifdef MSP

#include "msp430g2553.h"
#include <legacymsp430.h>
#include "../node.h"

inline void hwResetUSCI() {

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

void hwInitGlobal() {

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

}


// Initialize related stuff.

/**
 * function wordializeUSCI()
 * UART A (main comm channel with MASTER)
 *    (bit1 = MISO, bit2 = MOSI, BIT4 = SCLK)
 */

void hwInitializeUSCI() {

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

}


word hwTransfer(word s) {

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

}


#endif