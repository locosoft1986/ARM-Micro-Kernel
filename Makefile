#
# Makefile for ARM Micro Kernel
#
XCC     = gcc
AS	    = as
LD      = ld
CFLAGS  = -c -fPIC -Wall -I. -I./include -mcpu=arm920t -msoft-float
# -g: include hooks for gdb
# -c: only compile
# -mcpu=arm920t: generate code for the 920t architecture
# -fpic: emit position-independent code
# -Wall: report all warnings

ASFLAGS	= -mcpu=arm920t -mapcs-32
# -mapcs: always generate a complete stack frame

LDFLAGS = -init main -Map main.map -N  -T orex.ld -L/u/wbcowan/gnuarm-4.0.2/lib/gcc/arm-elf/4.0.2 -L./lib

OBJECTS = main.o
LIBS = nameserver.a task.a syscall.a klib.a
LIBS_CLEAN = $(patsubst %.a,%.clean,$(LIBS))
LIBS_INCLUDE = $(patsubst %.a,-l%,$(LIBS))

all: main.elf

%.s: %.c
	$(XCC) -S $(CFLAGS) -o $@ $<

$(OBJECTS): %.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

%.a: %
	$(MAKE) -C $< all

main.elf: $(LIBS) $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS_INCLUDE) -lgcc

%.clean: %
	$(MAKE) -C $< clean

clean: $(LIBS_CLEAN)
	-rm -f *.elf *.s $(OBJECTS) main.map

install:
	cp *.elf ~/cs452/tftp
	chmod a+r ~/cs452/tftp/*
