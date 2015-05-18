UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
# OSX settings
CC=clang
CFLAGS=-D DEBUG -pedantic-errors -Wall -std=c89 -O0 -g
LABFLAGS=-D DEBUG=0 -pedantic -Wall -std=c89 -O3
else
# assume linux otherwise
CC=gcc
CFLAGS=-D DEBUG -pedantic-errors -Wall -ansi -O0 -g
LABFLAGS=-D DEBUG=0 -pedantic -Wall -ansi -O4
endif

PROG=hoskell

all: lab

lab: lmain lutils
	$(CC) $(LABFLAGS) main.o utils.o -o $(PROG)

lmain: main.c utils.h defines.h
	$(CC) $(LABFLAGS) -c main.c

lutils: utils.c utils.h defines.h
	$(CC) $(LABFLAGS) -c utils.c

debug: dmain dutils
	$(CC) $(CFLAGS) main.o utils.o -o $(PROG)

dmain: main.c utils.h defines.h
	$(CC) $(CFLAGS) -c main.c

dutils: utils.c utils.h defines.h
	$(CC) $(CFLAGS) -c utils.c

clean:
	rm *.o

distclean: clean
	rm $(PROG)
