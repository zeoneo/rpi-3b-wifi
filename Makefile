
# Configure the cross-compiler to use the desired system root.
CC :=arm-none-eabi-gcc --sysroot=. -isystem=./include

CFLAGS?=-O2 -g -march=armv8-a+crc -mcpu=cortex-a53 -mfpu=crypto-neon-fp-armv8 -fpic -ffreestanding -nostdlib -nostartfiles
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

KERNEL_ARCH_CFLAGS= -march=armv8-a+crc -mfpu=crypto-neon-fp-armv8 -fpic -ffreestanding
KERNEL_ARCH_CPPFLAGS=
KERNEL_ARCH_LDFLAGS=
KERNEL_ARCH_LIBS=

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra -Werror -std=c11  -mfpu=neon -nostartfiles -mno-unaligned-access -fno-tree-loop-vectorize -fno-tree-slp-vectorize -Wno-address-of-packed-member
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lm -lgcc

KERNEL_OBJS=\
src/boot.o \
src/bss-clear.o \
src/dma.o \
src/doprnt.o \
src/frame_alloc.o \
src/emmc.o \
src/fat.o \
src/fat16.o \
src/fat32.o \
src/sd_card.o \
src/gpio.o \
src/kernel_alloc.o \
src/rpi-armtimer.o \
src/rpi-interrupts.o \
src/rpi-mailbox-interface.o \
src/rpi-mailbox.o \
src/systimer.o \
src/sdhost.o \
src/uart0.o \
src/memcmp.o \
src/memcpy.o \
src/memmove.o \
src/memset.o \
src/printf.o \
src/strlen.o \
src/plan9_emmc.o \
src/fork.o \
src/scheduler.o \
src/task_switch.o \
src/list.o \
src/kernel.o

ifeq ($(DISABLE_EXP), 1)
	CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
	CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
	LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
	LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)
else
	CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS) -D__enable_exp_
	CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS) -D__enable_exp_
	LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS) -D__enable_exp_
	LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS) -D__enable_exp_
	KERNEL_OBJS += src/plan9_ether4330.o
endif

OBJS= $(KERNEL_OBJS)

LINK_LIST=\
$(LDFLAGS) \
$(KERNEL_OBJS) \
$(LIBS) \

.PHONY: all clean
.SUFFIXES: .o .c .S

all: kernel8-32.img

kernel8-32.img:kernel8-32.elf
	@arm-none-eabi-objdump -D kernel8-32.elf > kernel8-32.list
	@arm-none-eabi-nm  kernel8-32.elf  > kernel8-32.map
	@echo "  OBJCOPY    $@"
	@arm-none-eabi-objcopy kernel8-32.elf -O binary kernel8-32.img

kernel8-32.elf: $(OBJS) ./linker.ld
	@echo "  LD    $@"
	@$(CC) -T ./linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	

.c.o:
	@echo "  CC    $@"
	@$(CC) -MD -c $< -o $@ -std=c11 $(CFLAGS) $(CPPFLAGS)

.S.o:
	@echo "  CC    $@"
	@$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f kernel8-32.elf kernel8-32.img
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d
	rm -f *.list *.map *.txt

-include $(OBJS:.o=.d)
