CC=msp430-gcc
LD=msp430-ld
CFLAGS=-O2 -Wall -g -mmcu=msp430g2553 -DMSP -DnodeId=$(NODEID) -L/usr/msp430/lib/ldscripts/msp430g2553 -I../../common/src -T/usr/msp430/lib/ldscripts/msp430.x 

OBJS=node.o hardware/msp430.c

all: $(OBJS)
	$(CC) $(CFLAGS) -o node.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr *.elf *.o 
