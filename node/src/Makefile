CC=msp430-gcc
LD=msp430-ld
CFLAGS=-O2 -Wall -g -mmcu=msp430g2553 -DMSP -L/usr/msp430/lib/ldscripts/msp430g2553 -T/usr/msp430/lib/ldscripts/msp430.x 

OBJS=node.o

all: $(OBJS)
	$(CC) $(CFLAGS) -o node.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr *.elf *.o 
