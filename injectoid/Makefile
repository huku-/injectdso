OBJS=symbol.o main.o
BIN=injectoid

# D=-DDEBUG -ggdb
D=-ggdb

CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -O2 $(D) -D_FILE_OFFSET_BITS=64
LDFLAGS=-pie

all: $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

clean:
	rm -fr $(OBJS) $(BIN) offsets.h

