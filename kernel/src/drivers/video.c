#include "drivers/video.h"

#include "kernel.h"

#include "stdio.h"

struct video_info_struct video_info;

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

void init_kernel_video(multiboot_info_t *mbi) {
    extract_framebuffer_info(mbi);
}