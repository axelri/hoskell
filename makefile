CC=gcc
CFLAGS=-pedantic-errors -Wall -std=c89 -O0 -g -c
LABFLAGS=-pedantic -Wall -ansi -O4 -c
PROG=hoskell

all: hoskell

hoskell: main.o utils.o
	$(CC) main.o utils.o -o $(PROG)

main.o: main.c utils.h defines.h
	$(CC) $(LABFLAGS) main.c

utils.o: utils.h defines.h
	$(CC) $(LABFLAGS) utils.c

clean:
	rm *.o

distclean: clean
	rm $(PROG)
