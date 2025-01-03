#include "drivers/vesa.h"

#include "kernel.h"

struct video_info_struct vesa_video_info;

static void extract_framebuffer_info(multiboot_info_t *mbi) {
  multiboot_tag_t *tag;
  for (
    tag = (multiboot_tag_t *) mbi->tags;
    tag->type != MULTIBOOT_TAG_TYPE_END;
    tag = (multiboot_tag_t *) ((uint8_t *) tag + ((tag->size + 7) & ~7))
    ) {
      if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
        struct multiboot_tag_framebuffer *tagfb = (struct multiboot_tag_framebuffer *) tag;

        // Assigning to the video info structure
        vesa_video_info.virt_addr = (uint64_t)KERNEL_VIDEO_MEMORY;
        vesa_video_info.addr = (uint64_t)tagfb->common.framebuffer_addr;
        vesa_video_info.width = tagfb->common.framebuffer_width;
        vesa_video_info.height = tagfb->common.framebuffer_height;
        vesa_video_info.bits = tagfb->common.framebuffer_bpp;
        vesa_video_info.pitch = tagfb->common.framebuffer_pitch;
        vesa_video_info.type = tagfb->common.framebuffer_type;
        
      }
  }
}

void init_kernel_vesa(multiboot_info_t *mbi) {
    extract_framebuffer_info(mbi);
}