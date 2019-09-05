
H		= .
M		= .
S		= $(H)

vpath %.c $(M):$(S)

ifndef GCCPREFIX
    GCCPREFIX   = c:\\MinGW\\msys\\1.0\\home\\wireb\\pic32-tools\\bin\\pic32-
endif

DEFS		+= -I. -I$(H)

LDRFILE		= bare-metal_32MX150F128D.ld 


DEPFLAGS	= -MT $@ -MD -MP -MF .deps/$*.dep
CFLAGS		= -O $(DEFS) $(DEPFLAGS)
ASFLAGS		= $(DEFS) $(DEPFLAGS)

DEVICE	= 32MX150F128D

PIC32PROG = /usr/local/bin/pic32prog
PIC32PROGCONF = 

CC		= $(GCCPREFIX)gcc -EL -g -mips32r2 -mprocessor=$(DEVICE) -O3
CC		+= -Wextra -Wall -fno-exceptions -ffunction-sections
CC		+= -fdata-sections -mno-smart-io

LDFLAGS         += -T $(LDRFILE) -lc -lm -Wl,-Map=picnc-5axis.map,--gc-sections

SIZE		= $(GCCPREFIX)size
OBJDUMP		= $(GCCPREFIX)objdump
OBJCOPY		= $(GCCPREFIX)objcopy
BIN2HEX		= $(GCCPREFIX)bin2hex

SRCOBJ	= main.o stepgen.o

.SUFFIXES:

.SUFFIXES:	.hex .dis .c .elf .o

all:		.deps picnc-5axis.elf picnc-5axis.hex
		$(SIZE) picnc-5axis.elf
clean:
		rm -rf .deps *.o *.elf *.bin *.dis *.map *.dep

veryclean:
		rm -rf .deps *.o *.elf *.bin *.dis *.map *.hex *.dep

.deps:
		mkdir .deps


picnc-5axis.elf:	$(SRCOBJ) $(LDRFILE)
		$(CC) $(LDFLAGS) $(SRCOBJ) -o $@
		chmod -x $@
		$(OBJDUMP) -d -S $@ > picnc-5axis.dis
		$(BIN2HEX) $@

load:           .deps picnc.elf
		$(PIC32PROG) $(PIC32PROGCONF) picnc-5axis.hex

.o.dis:
		$(OBJDUMP) -d -z -S $< > $@

ifeq (.deps, $(wildcard .deps))
-include .deps/*.dep
endif
