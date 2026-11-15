#include "drivers/video.h"

#include "kernel.h"

#include "stdio.h"

#include "memory/heap.h"

struct video_info_struct video_info;
struct video_rect_struct dirty_rect = {0xFFFFFFFF, 0xFFFFFFFF, 0, 0};

extern uint32_t mouse_x, mouse_y;

spinlock_t video_lock = {0};
spinlock_t dirty_lock = {0};

static const uint8_t mouse_cursor_bitmap[MOUSE_H][MOUSE_W] = {
    {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,2,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,2,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,2,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,2,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,2,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,2,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,2,0,0,0,0,0,0,0},
    {1,1,1,1,1,1,1,1,1,2,0,0,0,0,0,0},
    {1,1,1,1,1,2,2,2,2,2,0,0,0,0,0,0},
    {1,1,2,1,1,2,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,1,2,0,0,0,0,0,0,0,0,0},
    {2,2,0,2,1,1,2,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,1,1,2,0,0,0,0,0,0,0,0},
    {0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0}
};

static void extract_framebuffer_info(multiboot_info_t *mbi) 
{
  multiboot_tag_t *tag;
  for (
    tag = (multiboot_tag_t *) mbi->tags;
    tag->type != MULTIBOOT_TAG_TYPE_END;
    tag = (multiboot_tag_t *) ((uint8_t *) tag + ((tag->size + 7) & ~7))
    ) {
      if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
        struct multiboot_tag_framebuffer *tagfb = (struct multiboot_tag_framebuffer *) tag;

        // Assigning to the video info structure
        video_info.virt_addr = (uint64_t)KERNEL_VIDEO_MEMORY;
        video_info.addr = (uint64_t)tagfb->common.framebuffer_addr;
        video_info.width = tagfb->common.framebuffer_width;
        video_info.height = tagfb->common.framebuffer_height;
        video_info.bits = tagfb->common.framebuffer_bpp;
        video_info.pitch = tagfb->common.framebuffer_pitch;
        video_info.type = tagfb->common.framebuffer_type;
        video_info.r_pos = tagfb->framebuffer_red_field_position;
        video_info.g_pos = tagfb->framebuffer_green_field_position;
        video_info.b_pos = tagfb->framebuffer_blue_field_position;

        KLOG_INFO("VIDEO","Frambuffer virtual adress : 0x%llX", video_info.virt_addr);

        KLOG_INFO("VIDEO","Frambuffer adress : 0x%llX", video_info.addr);

        KLOG_INFO("VIDEO","Framebuffer width : %d", video_info.width);

        KLOG_INFO("VIDEO","Framebuffer height : %d", video_info.height);

        KLOG_INFO("VIDEO","Framebuffer bpp : %d", video_info.bits);

        KLOG_INFO("VIDEO","Framebuffer pitch : %d", video_info.pitch);

        KLOG_INFO("VIDEO","Framebuffer type : %d", video_info.type);
      }
  }
}

void init_kernel_video(multiboot_info_t *mbi) 
{
  extract_framebuffer_info(mbi);
}

void init_shadow_buffer()
{
  void* shadow_buf = kmalloc(video_info.pitch * video_info.height);
  video_info.stamp = kmalloc(video_info.width * 4);
  if(video_info.stamp && shadow_buf)
  {
    video_info.shadow_buf_addr = (uint64_t)shadow_buf;
    memcpy((uint64_t*)video_info.shadow_buf_addr, (uint64_t*)video_info.virt_addr, video_info.pitch * video_info.height);
    KLOG_INFO("VIDEO", "Successfully initialized shadow buffer");
  }
  else if(video_info.stamp)
  {
    kfree(video_info.stamp);
  }
  else if(shadow_buf)
  {
    kfree(shadow_buf);
  }
}

uint32_t* get_fb_address(){
  if(video_info.shadow_buf_addr)
  {
    return (uint32_t*)video_info.shadow_buf_addr;
  }
  else
  {
    return (uint32_t*)video_info.virt_addr;
  }

}

void dirty_add(int32_t xa, int32_t ya, int32_t xb, int32_t yb)
{

    int32_t w = (int32_t)video_info.width;
    int32_t h = (int32_t)video_info.height;

    // Sort corners, then make the max side exclusive
    int32_t x0 = xa < xb ? xa : xb;
    int32_t x1 = (xa < xb ? xb : xa) + 1;
    int32_t y0 = ya < yb ? ya : yb;
    int32_t y1 = (ya < yb ? yb : ya) + 1;

    // Clip to the screen
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;

    // Nothing visible left
    if (x0 >= x1 || y0 >= y1) return;

    uintptr_t flags = disable_interrupts();
    
    lock(&dirty_lock);

    // Grow the dirty rect (empty state is x0=y0=0xFFFFFFFF, x1=y1=0)
    if ((uint32_t)x0 < dirty_rect.x0) dirty_rect.x0 = (uint32_t)x0;
    if ((uint32_t)y0 < dirty_rect.y0) dirty_rect.y0 = (uint32_t)y0;
    if ((uint32_t)x1 > dirty_rect.x1) dirty_rect.x1 = (uint32_t)x1;
    if ((uint32_t)y1 > dirty_rect.y1) dirty_rect.y1 = (uint32_t)y1;

    unlock(&dirty_lock);

    restore_interrupts(flags);
}

void present()
{
  if(!video_info.shadow_buf_addr || (dirty_rect.x1 < dirty_rect.x0 || dirty_rect.y1 < dirty_rect.y0))
  {
    return;
  }

  uintptr_t flags = disable_interrupts();

  lock(&video_lock);

  uint32_t _mouse_x = mouse_x;
  uint32_t _mouse_y = mouse_y;

  lock(&dirty_lock);

  video_rect_t _dirty_rect = dirty_rect;

  dirty_rect.x0 = 0xFFFFFFFF;
  dirty_rect.y0 = 0xFFFFFFFF;
  dirty_rect.x1 = 0;
  dirty_rect.y1 = 0;

  unlock(&dirty_lock);

  for(uint32_t y = _dirty_rect.y0; y < _dirty_rect.y1; y++){
    if(_mouse_y <= y && y < _mouse_y + MOUSE_H)
    {
      memcpy((uint32_t*)(video_info.stamp + _dirty_rect.x0), (uint32_t*)((video_info.shadow_buf_addr) + y * video_info.pitch + _dirty_rect.x0 * 4), (_dirty_rect.x1 - _dirty_rect.x0) * 4);

      for(uint32_t x = max(0, (int32_t)_dirty_rect.x0); x < min(video_info.width, _dirty_rect.x1); x++)
      {
        if(_mouse_x <= x && x < _mouse_x + MOUSE_W)
        {
          uint8_t color_id = mouse_cursor_bitmap[y - _mouse_y][x - _mouse_x];

          if(color_id == 0) continue;

          uint32_t color = color_id == 2 ? 0x888888 : 0xFFFFFF;

          ((uint32_t*)video_info.stamp)[x] = color;
        }
      }
    
      memcpy((uint32_t*)((video_info.virt_addr) + y * video_info.pitch + _dirty_rect.x0 * 4), video_info.stamp + _dirty_rect.x0, (_dirty_rect.x1 - _dirty_rect.x0) * 4);
    }
    else
    {
      memcpy((uint32_t*)((video_info.virt_addr) + y * video_info.pitch + _dirty_rect.x0 * 4), (uint32_t*)((video_info.shadow_buf_addr) + y * video_info.pitch + _dirty_rect.x0 * 4), (_dirty_rect.x1 - _dirty_rect.x0) * 4);
    }
  }

  unlock(&video_lock);

  restore_interrupts(flags);
}