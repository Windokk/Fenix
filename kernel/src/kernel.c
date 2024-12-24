#include "multiboot2.h"

#include "kernel.h"

void HALT_AND_CATCH_FIRE(const char *fmt, ...) {

  DEBUG("wtf bro");
  x86_hlt(); 
  while(1) {}
}

void idle() {
  while(1) {
    x86_hlt();
  }
}

void kernel_premain(unsigned long magic, multiboot_info_t* mbi_phys){

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
        HALT_AND_CATCH_FIRE("--------- MAGIC NUMBER NOT RECOGNIZED ! ---------\nMAGIC NUMBER : 0x%X\n", (magic - KERNEL_START_MEMORY));
    }

    init_serial();
}

void kernel_main()
{   
    DEBUG("Lol\n");
    idle();
}