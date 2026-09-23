CC            = gcc
LD            = ld
AS            = gcc
OBJCOPY       = objcopy
QEMU          = qemu-system-x86_64
GRUB_MKRESCUE = grub-mkrescue

INCLUDES = -Ikernel/arch -Ikernel/drivers -Ikernel/mm -Ikernel/task \
           -Ikernel/fs -Ikernel/lib -Ikernel/gui

CFLAGS   = -m64 -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -mno-red-zone -nostdlib -Wall -Wextra -O2 $(INCLUDES) \
           -mno-sse -mno-sse2 -mno-mmx -mno-80387 -fno-omit-frame-pointer -g
ASFLAGS  = -m64
LDFLAGS  = -m elf_x86_64 -T boot/linker.ld -nostdlib

USER_CFLAGS = -m64 -ffreestanding -nostdlib -fno-pic -fno-pie \
              -fno-stack-protector -mno-red-zone -Iuser/lib \
              -mno-sse -mno-sse2 -mno-mmx -mno-80387 -O2 -Wall -g

C_SOURCES   = $(wildcard kernel/*.c) \
              $(wildcard kernel/arch/*.c) \
              $(wildcard kernel/drivers/*.c) \
              $(wildcard kernel/mm/*.c) \
              $(wildcard kernel/task/*.c) \
              $(wildcard kernel/fs/*.c) \
              $(wildcard kernel/lib/*.c) \
              $(wildcard kernel/gui/*.c)
ASM_SOURCES = $(wildcard kernel/arch/*.S) \
              $(wildcard kernel/task/*.S)

C_OBJS   = $(C_SOURCES:.c=.o)
ASM_OBJS = $(ASM_SOURCES:.S=.o)
BOOT_OBJ = boot/boot.o

USER_LIB_SRCS = user/lib/syscall.c \
                user/lib/string.c \
                user/lib/stdio.c \
                user/lib/stdlib.c
USER_LIB_OBJS = $(USER_LIB_SRCS:.c=.o)

OBJS   = $(BOOT_OBJ) $(C_OBJS) $(ASM_OBJS) user/init.elf.o user/hello.elf.o
KERNEL = myos.elf
ISO    = myos.iso
DISK   = disk.img

.PHONY: all clean run debug

all: $(ISO)

$(DISK):
	dd if=/dev/zero of=$(DISK) bs=1M count=16 2>/dev/null
	mkfs.fat -F 16 $(DISK)
	
user/%.o: user/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

user/init.elf: user/init.o $(USER_LIB_OBJS) user/user.ld
	$(LD) -m elf_x86_64 -T user/user.ld -nostdlib -o $@ \
	     user/init.o $(USER_LIB_OBJS)

user/hello.elf: user/hello.o $(USER_LIB_OBJS) user/hello.ld
	$(LD) -m elf_x86_64 -T user/hello.ld -nostdlib -o $@ \
	     user/hello.o $(USER_LIB_OBJS)

user/init.elf.o: user/init.elf
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.userelf,alloc,load,readonly,data,contents \
	    $< $@

user/hello.elf.o: user/hello.elf
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.userelf,alloc,load,readonly,data,contents \
	    $< $@

$(KERNEL): $(OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot/%.o: boot/%.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/%.o: kernel/%.S
	$(AS) $(ASFLAGS) -c $< -o $@

$(ISO): $(KERNEL) grub.cfg
	mkdir -p iso/boot/grub
	cp $(KERNEL) iso/boot/
	cp grub.cfg iso/boot/grub/
	$(GRUB_MKRESCUE) -o $@ iso

run: $(ISO) $(DISK)
	$(QEMU) -cdrom $(ISO) -boot d -serial stdio \
	        -bios /usr/share/ovmf/OVMF.fd \
	        -drive file=$(DISK),format=raw,if=ide,index=0,cache=writethrough

debug: $(ISO) $(DISK)
	$(QEMU) -cdrom $(ISO) -boot d -serial stdio -s -S \
	        -bios /usr/share/ovmf/OVMF.fd \
	        -drive file=$(DISK),format=raw,if=ide,index=0,cache=writethrough

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -f $(USER_LIB_OBJS)
	rm -f user/init.o  user/init.elf  user/init.elf.o
	rm -f user/hello.o user/hello.elf user/hello.elf.o
	rm -rf iso
