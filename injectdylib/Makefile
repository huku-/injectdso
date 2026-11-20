OBJS=memory.o symbol.o main.o
BIN=injectdylib
CFLAGS=-Wall -O2 -pedantic -g
LDFLAGS=
CC=cc

all: $(OBJS)
	$(CC) $(OBJS) -o $(BIN) $(LDFLAGS)

.PHONY: clean
clean:
	rm -fr $(OBJS) $(BIN) a.out *.core core *~
