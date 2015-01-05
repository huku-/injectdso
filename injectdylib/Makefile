OBJS=memory.o symbol.o main.o
BIN=injectdylib
CFLAGS=-Wall -O2 -g
LDFLAGS=
# CFLAGS=-Wall -O2 -g -m32
# LDFLAGS=-m32
CC=cc

all: $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

.PHONY: clean
clean:
	rm -fr $(OBJS) $(BIN) a.out *.core core *~
