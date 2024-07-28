#include "kernel.h"



#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/vga.h"



#define VIDEO_START 0xb8000
#define VGA_LIGHT_GRAY 7


#ifndef NULL
#define NULL ((void *)0)
#endif


#define ALIGN_UP(addr, align) ((addr + (align - 1)) & ~(align - 1))

struct multiboot_tag_framebuffer *fb_tag;

static void PrintString(char *str)
{
  unsigned char *video = ((unsigned char *)VIDEO_START);
  while(*str != '\0') {
    *(video++) = *str++;
    *(video++) = VGA_LIGHT_GRAY;
  }
}



void kernel_premain(struct multiboot_tag *multiboot_info){

    for (multiboot_tag_t *tag = multiboot_info + 1; tag->type != MULTIBOOT2_TAG_TYPE_END; tag = (multiboot_tag_t *)((uintptr_t)tag + ALIGN_UP(tag->size, 8)))
    {
        if (tag->type == MULTIBOOT2_TAG_TYPE_FRAMEBUFFER) {
            fb_tag = (struct multiboot_tag_framebuffer *)tag;
        }
    }
}

void kernel_main()
{   
    graphics_init();

    
    graphics_plot_pixel(graphics_get_global_rctx(), 30,30, 15);

    while (1);
}