# Define flags
CCFLAGS = -Wall -Wextra -ffreestanding -nostdlib -mno-red-zone -g -mcmodel=kernel -m64
ASFLAGS = -g
LDFLAGS = -nostdlib -mcmodel=kernel

# Define file paths
BUILD_DIR = build
SYSROOT = build/sysroot
INCDIR = $(SYSROOT)/include
LIBDIR = $(SYSROOT)/lib
BOOTDIR = $(SYSROOT)/boot
KERNELDIR= $(SYSROOT)/kernel

ISO_DIR = $(BUILD_DIR)/isodir

BOOT_O = $(BUILD_DIR)/boot/boot.o

FENIX_BIN = $(BUILD_DIR)/fenix.bin

ISO_FILE = fenix.iso

LIBC_BUILD_DIR = $(BUILD_DIR)/libc
LIBC_A = $(LIBC_BUILD_DIR)/libc.a
LIBC_SRC = $(shell find libc/src -type f \( -name "*.c" -or -name "*.s" \) )
LIBC_OBJ = $(patsubst libc/src/%,build/libc/%.o,$(LIBC_SRC))
LIBC_S_O = $(LIBC_BUILD_DIR)/%.s.o
LIBC_C_O = $(LIBC_BUILD_DIR)/%.c.o

LIBK_BUILD_DIR = $(BUILD_DIR)/libk
LIBK_A = $(LIBK_BUILD_DIR)/libk.a
LIBK_OBJ = $(patsubst libc/src/%,build/libk/%.o,$(LIBC_SRC))
LIBK_S_O = $(LIBK_BUILD_DIR)/%.s.o
LIBK_C_O = $(LIBK_BUILD_DIR)/%.c.o


KERNEL_SRC = $(shell find kernel/src -type f \( -name "*.c" -or -name "*.s" \) )
KERNEL_OBJ = $(patsubst kernel/src/%,build/kernel/%.o,$(KERNEL_SRC))
KERNEL_INC = $(shell find kernel/include -type f \( -name "*.h" \) )
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel
KERNEL_C_O = $(KERNEL_BUILD_DIR)/%.c.o
KERNEL_S_O = $(KERNEL_BUILD_DIR)/%.s.o

# Define compiler, archiver, assembler
CC = x86_64-elf-gcc
AR = x86_64-elf-ar
AS = x86_64-elf-as
LD = x86_64-elf-ld -L$(LIBDIR)

# Default target
all: $(ISO_FILE)

#Rule to build libc/Install it to sysroot
$(LIBC_A): headers $(LIBC_OBJ) $(LIBK_A)
	@mkdir -p $(@D)
	@$(AR) rcs $(LIBC_A) $(LIBC_OBJ)
	@mkdir -p "$(LIBDIR)"
	@cp $@ "$(LIBDIR)"
	@cp $(LIBK_A) "$(LIBDIR)"
	@echo "Built libc.a"

$(LIBC_S_O): libc/src/%.s
	@mkdir -p "$(@D)"
	@$(AS) $(ASFLAGS) -o $@ $<

$(LIBC_C_O): libc/src/%.c
	@mkdir -p "$(@D)"
	@$(CC) $(CCFLAGS) -I$(INCDIR) -D__IS_LIBC -mcmodel=large -MD -c -o $@ $<
headers: 
	@mkdir -p "$(INCDIR)"
	@cp -R libc/include $(SYSROOT)
	@echo "Copied headers into sysroot"
#


$(LIBK_A): $(LIBK_OBJ)
	@mkdir -p $(@D)
	@$(AR) rcs $(LIBK_A) $(LIBK_OBJ)
	@echo "Built libk.a"
$(LIBK_S_O): libc/src/%.s
	@mkdir -p "$(@D)"
	@$(AS) $(ASFLAGS) -o $@ $<
$(LIBK_C_O): libc/src/%.c
	@mkdir -p "$(@D)"
	@$(CC) $(CCFLAGS) -I$(INCDIR) -Ikernel/include -D__IS_LIBK -mcmodel=large -MD -c -o $@ $<


# Rule to build boot.o
$(BOOT_O): kernel/src/boot/boot.S
	@mkdir -p $(@D)
	@$(CC) -c $(CCFLAGS) -o $@ $^
	@echo "Built boot.o"
#




# Rule to build kernel's .s.o files
$(KERNEL_S_O): kernel/src/%.S
	@mkdir -p "$(dir $@)"
	$(AS) -c $(ASFLAGS) -o $@ $<
# Rule to build kernel's .c.o files
$(KERNEL_C_O): kernel/src/%.c
	@mkdir -p "$(dir $@)"
	$(CC) -c $(CCFLAGS) -L$(LIBDIR) -I$(INCDIR) -Ikernel/include -o $@ $<
#


# Rule to link boot.o and kernel.o to create fenix.bin
$(FENIX_BIN): $(BOOT_O) $(KERNEL_OBJ) build/libc/libc.a build/libk/libk.a
	@$(CC) -I$(INCDIR) $(LDFLAGS) -T linker.ld -o $@ $^ 
	@echo "Built fenix.bin"
	
#



# Rule to create the ISO file
$(ISO_FILE): $(LIBC_A) $(FENIX_BIN) 
	@if grub2-file --is-x86-multiboot2 build/fenix.bin; then \
		echo "Multiboot confirmed"; \
		mkdir -p build/isodir; \
		mkdir -p build/isodir/boot; \
		cp build/fenix.bin build/isodir/boot/fenix.bin; \
		mkdir -p build/isodir/boot/grub; \
		echo menuentry \"Fenix\" { > build/isodir/boot/grub/grub.cfg; \
		echo "	multiboot2 /boot/fenix.bin" >> build/isodir/boot/grub/grub.cfg; \
		echo "}" >> build/isodir/boot/grub/grub.cfg; \
		grub2-mkrescue -o fenix.iso build/isodir; \
		mkdir -p $(BOOTDIR)/grub; \
		cp fenix.iso $(BOOTDIR)/fenix.iso; \
		cp build/isodir/boot/grub/grub.cfg $(BOOTDIR)/grub/grub.cfg; \
	else \
		echo "The file is not multiboot"; \
	fi \

#


# Clean rule to remove build artifacts
clean:
	@rm -rf $(BUILD_DIR) $(ISO_FILE)

.PHONY: all clean