CFLAGS=-Wall -O0 -c -fPIC
CC=cc

all: i386_stub.o x86_64_stub.o

i386_stub.o: i386_stub.S
	$(CC) $(CFLAGS) -m32 $< -o $@

x86_64_stub.o: x86_64_stub.S
	$(CC) $(CFLAGS) -m64 $< -o $@

.PHONY: clean
clean:
	rm -fr *.o a.out *.core core *~
