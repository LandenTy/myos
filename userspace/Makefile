CC     = $(HOME)/cross/tools/bin/i686-elf-gcc
ASM    = nasm
LD     = $(HOME)/cross/tools/bin/i686-elf-ld
CFLAGS  = -m32 -ffreestanding -fno-stack-protector -fno-builtin \
          -nostdlib -O2 -Ilib
LDFLAGS = -T lib/user.ld -m elf_i386
BUILD = build

# Auto-discover all program .c files (excludes lib/)
SRCS     := $(wildcard *.c)
PROGRAMS := $(patsubst %.c, $(BUILD)/%.elf, $(SRCS))

all: $(BUILD) $(BUILD)/crt0.o $(BUILD)/stdlib.o $(PROGRAMS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/crt0.o: lib/crt0.asm
	$(ASM) -f elf32 $< -o $@

$(BUILD)/stdlib.o: lib/stdlib.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link crt0 + stdlib + program object into ELF
$(BUILD)/%.elf: $(BUILD)/%.o $(BUILD)/crt0.o $(BUILD)/stdlib.o
	$(LD) $(LDFLAGS) -o $@ $(BUILD)/crt0.o $< $(BUILD)/stdlib.o

clean:
	rm -rf $(BUILD)

.PHONY: all clean

