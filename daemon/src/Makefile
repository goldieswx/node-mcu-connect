CFLAGS=-lbcm2835 -I../../common/src


all: daemon.c ../../common/src/fifo.c
	gcc $(CFLAGS) -o mydaemon daemon.c .

