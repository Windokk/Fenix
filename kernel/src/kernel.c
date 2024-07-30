#include "kernel.h"



#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "boot/multiboot2.h"

#include "drivers/vga.h"
#include "graphics/graphics.h"

#define COM1 0x3F8


static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}


void init_serial() {
    outb(COM1 + 1, 0x00);    // Disable all interrupts
    outb(COM1 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(COM1 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(COM1 + 1, 0x00);    //                  (hi byte)
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int serial_received() {
    return inb(COM1 + 5) & 1;
}

char read_serial() {
    while (serial_received() == 0);

    return inb(COM1);
}

int is_transmit_empty() {
    return inb(COM1 + 5) & 0x20;
}

void write_serial(char a) {
    while (is_transmit_empty() == 0);

    outb(COM1, a);
}

void serial_print(const char *str) {
    while (*str) {
        write_serial(*str++);
    }
}



void debug_print(const char *msg) {
    serial_print(msg);
}

char* itoa(int value) {
    static char buffer[12]; // Enough to hold -2147483648 and null terminator
    char *ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';

    int is_negative = value < 0;
    if (is_negative) {
        value = -value;
    }

    do {
        *--ptr = '0' + (value % 10);
        value /= 10;
    } while (value);

    if (is_negative) {
        *--ptr = '-';
    }

    return ptr;
}

char* itoa_hex(uint32_t value, char* str) {
    const char *digits = "0123456789ABCDEF";
    char *ptr = str;
    int i;
    
    // Process each nibble (4 bits) of the value
    for (i = 28; i >= 0; i -= 4) {
        *ptr++ = digits[(value >> i) & 0xF];
    }
    
    *ptr = '\0'; // Null-terminate the string
    return str;
}

struct multiboot_tag_framebuffer *fb_tag;

void kernel_premain(uint32_t magic, unsigned long addr){
    init_serial();
    struct multiboot_tag *tag;
    unsigned size;
    size = *(unsigned *) addr;

    if(magic != MULTIBOOT2_BOOTLOADER_MAGIC){
      char magic_buf[12];
      debug_print("Error, wrong magic number : 0x");
      debug_print(itoa_hex(magic, magic_buf));
      debug_print("\n");
    }

    for (tag = (struct multiboot_tag *) (addr + 8);
       tag->type != MULTIBOOT_TAG_TYPE_END;
       tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag 
                                       + ((tag->size + 7) & ~7)))
    {
      switch (tag->type){
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
          {
            multiboot_uint32_t color;
            unsigned i;
            struct multiboot_tag_framebuffer *tagfb
              = (struct multiboot_tag_framebuffer *) tag;
            void *fb = (void *) (unsigned long) tagfb->common.framebuffer_addr;
            unsigned int x = 30;
            unsigned int y = 30;
            unsigned int pitch;
            unsigned int width;
            unsigned int height;
            unsigned int bpp;
            unsigned int fb_addr;
            switch (tagfb->common.framebuffer_type)
              {
              case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
                {
                  unsigned best_distance, distance;
                  struct multiboot_color *palette;
            
                  palette = tagfb->framebuffer_palette;

                  color = 0;
                  best_distance = 4*256*256;
            
                  for (i = 0; i < tagfb->framebuffer_palette_num_colors; i++)
                    {
                      distance = (0xff - palette[i].blue) 
                        * (0xff - palette[i].blue)
                        + palette[i].red * palette[i].red
                        + palette[i].green * palette[i].green;
                      if (distance < best_distance)
                        {
                          color = i;
                          best_distance = distance;
                        }
                    }
                  
                }
                break;

              case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
                color = ((1 << tagfb->framebuffer_blue_mask_size) - 1) 
                  << tagfb->framebuffer_blue_field_position;

                pitch = tagfb->common.framebuffer_pitch;
                width = tagfb->common.framebuffer_width;
                height = tagfb->common.framebuffer_height;
                bpp = tagfb->common.framebuffer_bpp;
                fb_addr = (unsigned int)fb;

                // For RGB framebuffer with 32 bpp
                unsigned int color = 0xFFFFFFFF; // White color (ARGB)

                unsigned int *pixel = (unsigned int *)(fb_addr + y * pitch + x * 4);
                
                debug_print("Framebuffer info received\n");
                debug_print("\nFramebuffer address: 0x");
                char buf[12];
                debug_print(itoa_hex(fb_addr, buf));
                debug_print(", Width: ");
                debug_print(itoa(width));
                debug_print(", Height: ");
                debug_print(itoa(height));
                debug_print(", Pitch: ");
                debug_print(itoa(pitch));
                debug_print(", BPP: ");
                debug_print(itoa(bpp));
                debug_print("\n");
                
                *pixel = color;

                break;

              case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                color = '\\' | 0x0100;
                break;

              default:
                color = 0xffffffff;
                break;
              }
            
            
            
            break;
          }

      }

    }
}

void kernel_main()
{   
    while(1){
        asm volatile ("hlt");
    }
}