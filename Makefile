# Define flags
CCFLAGS = -Wall -Wextra -ffreestanding -nostdlib -mno-red-zone -g -mcmodel=large -m64 -fstack-protector-strong -DKERNEL_PROFILING -D__FENIX__
ASFLAGS = -g
LDFLAGS = -nostdlib -mcmodel=large
NASM_FLAGS := -w-number-overflow -felf64 -g

# Define file paths
BUILD_DIR = build
SYSROOT = build/sysroot
INCDIR = $(SYSROOT)/include
LIBDIR = $(SYSROOT)/lib
BOOTDIR = $(SYSROOT)/boot
KERNELDIR= $(SYSROOT)/kernel

ISO_DIR = $(BUILD_DIR)/isodir

FENIX_BIN = $(BUILD_DIR)/fenix.bin

ISO_FILE = fenix.iso

KERNEL_SRC = $(shell find kernel/src -type f -name "*.c")
KERNEL_SRC += $(shell find kernel/src -type f -name "*.asm" -not -path "*/boot/*")
KERNEL_OBJ = $(patsubst kernel/src/%,build/kernel/%.o,$(KERNEL_SRC))
KERNEL_INC = $(shell find kernel/include -type f \( -name "*.h" \) )
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel
KERNEL_C_O = $(KERNEL_BUILD_DIR)/%.c.o
KERNEL_ASM_O = $(KERNEL_BUILD_DIR)/%.asm.o

UACPI_SRC = $(shell find kernel/vendor/uacpi/source -type f -name "*.c")
UACPI_OBJ = $(patsubst kernel/vendor/uacpi/source/%.c,build/uacpi/%.c.o,$(UACPI_SRC))
UACPI_FLAGS = -DUACPI_OVERRIDE_TYPES \
              -Ikernel/vendor/uacpi/include -Ikernel/include

BOOT_ASM_SRC = $(shell find kernel/src/boot -type f \( -name "*.asm" \) )
BOOT_ASM_OBJ = $(patsubst kernel/src/boot/%.asm, build/boot/%.o, $(BOOT_ASM_SRC))



# Define compiler, archiver, assembler, linker, NASM
CC = x86_64-elf-gcc
AR = x86_64-elf-ar
AS = x86_64-elf-as
LD = x86_64-elf-ld -L$(LIBDIR)
NASM = nasm

# Default target
all: $(ISO_FILE)


# Rule to build assembly files
build/boot/%.o: kernel/src/boot/%.asm
	@mkdir -p $(dir $@)
	@$(NASM) $(NASM_FLAGS) $< -o $@
#

# Rule to build kernel's assembly files
$(KERNEL_ASM_O): kernel/src/%.asm
	@mkdir -p "$(dir $@)"
	@$(NASM) $(NASM_FLAGS) -o $@ $<
#

# Rule to build kernel's .c.o files
# uACPI include path and type override are added globally so that uacpi_impl.c
# (which lives in kernel/src/) can include <uacpi/...> headers.
$(KERNEL_C_O): kernel/src/%.c
	@mkdir -p "$(dir $@)"
	@$(CC) -c $(CCFLAGS) -L$(LIBDIR) -I$(INCDIR) -Ikernel/include \
	       -DUACPI_OVERRIDE_TYPES -Ikernel/vendor/uacpi/include -o $@ $<
#

# Rule to build uACPI vendor sources
build/uacpi/%.c.o: kernel/vendor/uacpi/source/%.c
	@mkdir -p "$(dir $@)"
	@$(CC) -c $(CCFLAGS) $(UACPI_FLAGS) -o $@ $<
#


# Rule to link boot.o and kernel.o to create fenix.bin
$(FENIX_BIN): $(KERNEL_OBJ) $(BOOT_ASM_OBJ) $(UACPI_OBJ)
	@$(CC) -I$(INCDIR) $(LDFLAGS) -T linker.ld -o $@ $^
	@echo "Built fenix.bin"
	
#



# Rule to create the ISO file
$(ISO_FILE): $(FENIX_BIN)
	@if grub-file --is-x86-multiboot2 build/fenix.bin; then \
		echo "Multiboot confirmed"; \
		mkdir -p build/isodir; \
		mkdir -p build/isodir/boot; \
		cp build/fenix.bin build/isodir/boot/fenix.bin; \
		mkdir -p build/isodir/boot/grub; \
		echo menuentry \"Fenix\" { > build/isodir/boot/grub/grub.cfg; \
		echo "	multiboot2 /boot/fenix.bin" >> build/isodir/boot/grub/grub.cfg; \
		echo "}" >> build/isodir/boot/grub/grub.cfg; \
		grub-mkrescue -o build/fenix.iso build/isodir; \
		mkdir -p $(BOOTDIR)/grub; \
		cp build/fenix.iso $(BOOTDIR)/fenix.iso; \
		cp build/isodir/boot/grub/grub.cfg $(BOOTDIR)/grub/grub.cfg; \
	else \
		echo "The file is not multiboot"; \
	fi \

#


# Clean rule to remove build artifacts
clean:
	@rm -rf $(BUILD_DIR)

.PHONY: all clean