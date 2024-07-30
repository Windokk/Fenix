#ifndef __KERNEL_H_
#define __KERNEL_H_




extern struct multiboot_tag_framebuffer *fb_tag;

void debug_print(const char *msg);

#endif // _KERNEL_H_