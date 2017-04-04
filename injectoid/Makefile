OBJS=main.o
BIN=injectoid

# D=-DDEBUG -ggdb
D=-ggdb

CC=$(CROSS_COMPILE)gcc

CFLAGS=-Wall -O2 $(D) -D_FILE_OFFSET_BITS=64
LDFLAGS=-pie

all: offsets $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

offsets:
	. offsets.sh

clean:
	rm -fr $(OBJS) $(BIN) offsets.h

