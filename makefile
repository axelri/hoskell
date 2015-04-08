CC=clang
CFLAGS=-pedantic-errors -Wall -Werror -std=c89 -O0 -g -c
PROG=hoskell

all: hoskell

hoskell: main.o
	$(CC) main.o -o $(PROG)

main.o: main.c
	$(CC) $(CFLAGS) main.c

clean:
	rm *.o

distclean: clean
	rm $(PROG)
