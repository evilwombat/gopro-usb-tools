CC=../../../goprokernel/arm-2009q3/bin/arm-none-linux-gnueabi-gcc
LD=../../../goprokernel/arm-2009q3/bin/arm-none-linux-gnueabi-ld
OBJCOPY=../../../goprokernel/arm-2009q3/bin/arm-none-linux-gnueabi-objcopy
CFLAGS=-mcpu=cortex-a9

LDFLAGS=$(CFLAGS)

TARGETS=evilcortex

all : $(TARGETS)

evilcortex : evilcortex.S
	$(CC) $(CFLAGS) -c -o evilcortex.o evilcortex.S
	$(OBJCOPY) -O binary evilcortex.o evilcortex

clean :
	rm -rf *.o $(TARGETS)
