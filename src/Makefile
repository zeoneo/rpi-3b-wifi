
# Configure the cross-compiler to use the desired system root.
CC :=arm-none-eabi-gcc --sysroot=. -isystem=./include
AR :=arm-none-eabi-ar
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
boot.o \
bss-clear.o \
dma.o \
doprnt.o \
frame_alloc.o \
emmc.o \
fat.o \
fat16.o \
fat32.o \
sd_card.o \
gpio.o \
kernel_alloc.o \
rpi-armtimer.o \
rpi-interrupts.o \
rpi-mailbox-interface.o \
rpi-mailbox.o \
systimer.o \
sdhost.o \
uart0.o \
memcmp.o \
memcpy.o \
memmove.o \
memset.o \
strstr.o \
cstring.o \
string.o \
printf.o \
vsnprintf.o \
strlen.o \
plan9_emmc.o \
fork.o \
scheduler.o \
task_switch.o \
list.o \
queue.o \
lock.o \

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
	KERNEL_OBJS += plan9_ether4330.o
	KERNEL_OBJS += etherif.o
	KERNEL_OBJS += bcm4343.o
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

kernel8-32.elf: librpi3b.a kernel.o ../linker.ld
	@echo "  LD    $@"
	@$(CC) -T ../linker.ld -o $@  $(LDFLAGS)  kernel.o -L . -lrpi3b $(LIBS)
	
librpi3b.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) rc $@ $(OBJS)

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
	rm -f *.a

format:
	@find . -type f -name "*.c" -exec clang-format -style=file -i {} \;
	@find . -type f -name "*.h" -exec clang-format -style=file -i {} \;

-include $(OBJS:.o=.d)
