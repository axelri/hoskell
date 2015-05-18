UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
# OSX settings
CC=clang
CFLAGS=-pedantic-errors -Wall -std=c89 -O0 -g -c
LABFLAGS=-pedantic -Wall -std=c89 -O3 -c
else
# assume linux otherwise
CC=gcc
CFLAGS=-pedantic-errors -Wall -ansi -O0 -g -c
LABFLAGS=-pedantic -Wall -ansi -O4 -c
endif

PROG=hoskell

all: hoskell

hoskell: main.o utils.o
	$(CC) main.o utils.o -o $(PROG)

main.o: main.c utils.h defines.h
	$(CC) $(LABFLAGS) main.c

utils.o: utils.c utils.h defines.h
	$(CC) $(LABFLAGS) utils.c

clean:
	rm *.o

distclean: clean
	rm $(PROG)
