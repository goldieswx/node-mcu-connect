CC=msp430-gcc
CFLAGS=-Os -Wall -g -mmcu=msp430g2553 -L/usr/msp430/lib/ldscripts/msp430g2553 -T/usr/msp430/lib/ldscripts/msp430.x 

OBJS=adc-to-slave.o
SLAVE=spi-rb-protocol.o
ADC=adc-to-slave.o

slave: $(SLAVE)
	$(CC) $(CFLAGS) -o slave.elf $(SLAVE)

adc: $(ADC)
	$(CC) $(CFLAGS) -o adc.elf $(ADC)
 

all: $(OBJS)
	$(CC) $(CFLAGS) -o main.elf $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -fr *.elf *.o 
