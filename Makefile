CC=gcc
CFLAGS=-g -Wall

all: follow libfollow.a libfollowleakcheck.a

follow: follow.c
	$(CC) -o follow follow.c $(CFLAGS)

bt.o: bt.c bt.h
	$(CC) -c bt.c $(CFLAGS)

btleakcheck.o: bt.c bt.h
	$(CC) -c bt.c -o btleakcheck.o -DLEAKCHECK $(CFLAGS)

leakcheck.o: leakcheck.c
	$(CC) -c leakcheck.c -DLIBFOLLOW $(CFLAGS)

libfollow.a: bt.o
	ar r libfollow.a bt.o 

libfollowleakcheck.a: btleakcheck.o leakcheck.o
	ar r libfollowleakcheck.a btleakcheck.o leakcheck.o

clean:
	rm -f *.o *.a follow
