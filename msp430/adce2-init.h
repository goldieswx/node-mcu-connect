
#define LOW_POWER_MODE(a)    (__bis_SR_register(LPM3_bits + GIE))
#define INTERRUPT 			 (P2IN & CS_INCOMING_PACKET)