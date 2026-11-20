CFLAGS=-Wall -O0 -c -fPIC
CC=cc

arm64_stub.bin: arm64_stub.S
	$(CC) $(CFLAGS) $< -o $@

i386_stub.bin: i386_stub.S
	$(CC) $(CFLAGS) -m32 $< -o $@

x86_64_stub.bin: x86_64_stub.S
	$(CC) $(CFLAGS) -m64 $< -o $@

.PHONY: clean
clean:
	rm -fr *.bin *.o a.out *.core core *~
