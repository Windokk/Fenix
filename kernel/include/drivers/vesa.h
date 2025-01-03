#ifndef __VESA_H
#define __VESA_H

#include "multiboot2.h"
#include "common.h"

#define KERNEL_VIDEO_MEMORY 0xFFFFC00000000000

#define VIDEO_INFO_MEM_SIZE(vi) (vi.width * vi.height * (vi.bits/8))

typedef struct video_info_struct {
  uint64_t virt_addr;
  uint64_t addr;
  int width;
  int height;
  int bits;
  int pitch;
  uint8_t  type;
} video_info_t;

extern struct video_info_struct vesa_video_info;

void init_kernel_vesa(multiboot_info_t *mbi);

#endif