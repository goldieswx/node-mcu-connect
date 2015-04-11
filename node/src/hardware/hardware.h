
#ifdef MSP
	#include "msp430.h"
#endif


inline void hwResetUSCI();
void hwInitGlobal();
word hwTransfer(word s);
void hwInitializeUSCI();