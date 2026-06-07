#pragma once

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
  uint8_t type;
  bool mapped;
  uint8_t r_pos;
  uint8_t g_pos;
  uint8_t b_pos;
} video_info_t;

extern struct video_info_struct video_info;

void init_kernel_video(multiboot_info_t *mbi);