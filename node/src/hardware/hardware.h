
#ifdef MSP
	#include "msp430.h"
	#include "msp430g2553.h"
	#include <legacymsp430.h>
#endif


inline void hwResetUSCI();
inline void hwExtSetClearInterrupt(int state, int extId);

void 		hwInitGlobal();
word 		hwTransfer(word s);
void 		hwInitializeUSCI();
