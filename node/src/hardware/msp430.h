#ifdef MSP

#define SWITCH_LOW_POWER_MODE   __bis_SR_register(LPM3_bits + GIE)
#define ENABLE_INTERRUPT 		__enable_interrupt()
#define DISABLE_INTERRUPT 		__disable_interrupt()
#define RESET 					WDTCTL = WDTHOLD
#define DISABLE_WATCHDOG(a)  	WDTCTL = WDTPW + WDTHOLD    

#endif