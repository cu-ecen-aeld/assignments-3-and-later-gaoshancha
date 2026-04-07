CC := $(CROSS_COMPILE)gcc
BINARY := aesdsocket

all: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -Wall -Werror -g  aesdsocket.o -o $(BINARY)

aesdsocket.o: aesdsocket.c
	$(CC) -Wall -Werror -g -c aesdsocket.c

clean:
	rm -f *.o $(BINARY)

.PHONY: all clean
