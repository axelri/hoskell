CC=gcc
CFLAGS=-pedantic-errors -Wall -std=c89 -O0 -g -c
LABFLAGS=-pedantic -Wall -ansi -O4 -c
PROG=hoskell

all: hoskell

hoskell: main.o
	$(CC) main.o -o $(PROG)

main.o: main.c
	$(CC) $(LABFLAGS) main.c

clean:
	rm *.o

distclean: clean
	rm $(PROG)
