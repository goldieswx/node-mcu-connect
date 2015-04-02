CC=msp430-gcc
LD=msp430-ld
CFLAGS=-O2 -Wall -g -mmcu=msp430g2553 -DMSP -L/usr/msp430/lib/ldscripts/msp430g2553 -T/usr/msp430/lib/ldscripts/msp430.x -I../../../common/src -I../../../common/src/hardware
 
OBJ=../../../common/src/hardware/msp-utils.o adce2-init.o adce2.o


%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

adce.elf: $(OBJ) 
	$(CC) -o $@ $^ $(CFLAGS)

clean: 
	rm -f *.o adce.elf 
