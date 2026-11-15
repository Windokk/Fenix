#pragma once

#include "multiboot2.h"
#include "common.h"

#define KERNEL_VIDEO_MEMORY 0xFFFFC00000000000

#define VIDEO_INFO_MEM_SIZE(vi) (vi.width * vi.height * (vi.bits/8))

// Arrow cursor (16 wide x 16 tall): 0=transparent, 1=white, 2=black outline
#define MOUSE_W 16
#define MOUSE_H 16

typedef struct video_info_struct {
  uint64_t virt_addr;
  uint64_t addr;
  uint64_t shadow_buf_addr;
  uint32_t width;
  uint32_t height;
  uint32_t bits;
  uint32_t pitch;
  uint8_t type;
  bool mapped;
  uint8_t r_pos;
  uint8_t g_pos;
  uint8_t b_pos;
  uint32_t* stamp;
} video_info_t;

typedef struct video_rect_struct {
  uint32_t x0;
  uint32_t y0;
  uint32_t x1;
  uint32_t y1;

} video_rect_t;

extern struct video_info_struct video_info;

void init_kernel_video(multiboot_info_t *mbi);
void init_shadow_buffer();
uint32_t* get_fb_address();
void present();
void dirty_add(int32_t xa, int32_t ya, int32_t xb, int32_t yb);