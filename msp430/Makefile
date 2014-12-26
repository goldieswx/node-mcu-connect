CC=msp430-gcc
LD=msp430-ld
CFLAGS=-O2 -Wall -g -mmcu=msp430g2553 -DMSP -L/usr/msp430/lib/ldscripts/msp430g2553 -T/usr/msp430/lib/ldscripts/msp430.x 

OBJS=node-spi.o
SLAVE=node-spi.o
ADC2=adce2.o msp-utils.o adce2-init.o
TEST=test.o

slave: $(SLAVE)
	$(CC) $(CFLAGS) -o slave.elf $(SLAVE)

adc2: $(ADC2)
	$(CC) $(CFLAGS) -o adc.elf $(ADC2)

test: $(TEST)
	$(CC) $(CFLAGS) -o test.elf $(TEST)
 

all: $(OBJS)
	$(CC) $(CFLAGS) -o main.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr *.elf *.o 
