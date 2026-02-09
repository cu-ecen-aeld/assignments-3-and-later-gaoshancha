CC := $(CROSS_COMPILE)gcc
BINARY := writer

all: writer

writer: writer.o
	$(CC) -Wall -Werror -g  writer.o -o $(BINARY)

writer.o: writer.c
	$(CC) -Wall -Werror -g -c writer.c

clean:
	rm -f *.o $(BINARY)

.PHONY: all clean
