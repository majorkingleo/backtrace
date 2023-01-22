CFLAGS=-g -Wall
CC=gcc

all: leaktest

leaktest.o: leaktest.c
	$(CC) -c leaktest.c $(CFLAGS)

leaktest: leaktest.o
	$(CC) -o leaktest leaktest.c $(CFLAGS)

clean:
	rm -f *.o leaktest
