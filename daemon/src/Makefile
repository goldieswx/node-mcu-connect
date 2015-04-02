CFLAGS=-lbcm2835 -I../../common/src
OBJ=daemon.o ../../common/src/fifo.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

mcuconnectd: $(OBJ) 
	gcc -o $@ $^ $(CFLAGS)

clean: 
	rm -f *.o mcuconnectd
