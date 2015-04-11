#ifdef MSP

#define SWITCH_LOW_POWER_MODE   			__bis_SR_register(LPM3_bits + GIE)

#define ENABLE_INTERRUPT 					__enable_interrupt()
#define DISABLE_INTERRUPT 					__disable_interrupt()

#define RESET 								WDTCTL = WDTHOLD
#define DISABLE_WATCHDOG(a)  				WDTCTL = WDTPW + WDTHOLD    

#define SIGNAL_EXT(extId)					P2OUT |= extId
#define STOP_SIGNAL_EXT(extId)				P2OUT &= ~extId

#define EXT1_INTR							(P1IN & BIT3)
#define EXT2_INTR 							(P2IN & BIT4)

#define TRIGGER_DEAMON_MESSAGE(nodeId)		UCA0TXBUF = 0x80 | nodeId

#define WAIT_COMM_RTS						while (IFG2 & UCA0TXIFG)  { /*P2OUT ^= BIT7;*/ __delay_cycles(50); }  

#define DELAY 								__delay_cycles

#endif