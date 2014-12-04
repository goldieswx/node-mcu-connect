CC=msp430-gcc
LD=msp430-ld
CFLAGS=-O2 -Wall -g -mmcu=msp430g2553 -L/usr/msp430/lib/ldscripts/msp430g2553 -T/usr/msp430/lib/ldscripts/msp430.x 

OBJS=node-spi.o
SLAVE=node-spi.o
ADC=adce.o msp-utils.o
TEST=test.o

slave: $(SLAVE)
	$(CC) $(CFLAGS) -o slave.elf $(SLAVE)

adc: $(ADC)
	$(CC) $(CFLAGS) -o adc.elf $(ADC)

test: $(TEST)
	$(CC) $(CFLAGS) -o test.elf $(TEST)
 

all: $(OBJS)
	$(CC) $(CFLAGS) -o main.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr *.elf *.o 
