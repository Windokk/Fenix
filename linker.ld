ENTRY(_start)

SECTIONS
{
  . = 4M;
  _kernel_physical_start = .;

  .boottext :
    {
      build/boot/boot.o (.multiboot)
      build/boot/boot.o (.text)
    }
  .bootrodata :
    {
      build/boot/boot.o (.rodata)
    }
  .bootdata :
    {
      build/boot/boot.o (.data)
    }
  .bootbss :
    {
      build/boot/boot.o (.bss)
      build/boot/boot.o (COMMON)
    }

  . = ALIGN(0x1000);
  _boot_end = .;

  . += 0xFFFFFFFF80000000;
  _kernel_virtual_start = .;
  .text : AT(_boot_end)
  {
    *(.multiboot)
    *(.text)
  }

  . = ALIGN(0x1000);

  .rodata : AT ( (LOADADDR (.text) + SIZEOF (.text) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(.rodata)
  }

  . = ALIGN(0x1000);

  .eh_frame : AT ( (LOADADDR (.rodata) + SIZEOF (.rodata) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(.eh_frame)
		*(.eh_frame.*)
  }

  . = ALIGN(0x1000);

  .data : AT ( (LOADADDR (.eh_frame) + SIZEOF (.eh_frame) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(.data)
  }

  . = ALIGN(0x1000);

  .bss : AT ( (LOADADDR (.data) + SIZEOF (.data) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(COMMON)
    *(.bss)
  }

  _kernel_virtual_end = .;

  _kernel_physical_end = (LOADADDR (.bss) + SIZEOF (.bss) + 0xFFF) & 0xFFFFFFFFFFFFF000;
}