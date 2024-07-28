#include "kernel.h"

#include <string.h>

struct render_context global_rctx;

struct render_context* graphics_get_global_rctx(void)
{
	return &global_rctx;
}

struct color graphics_color_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	struct color color;
	color.r = r;
	color.g = g;
	color.b = b;
	return color;
}

struct color graphics_color(uint32_t rgb)
{
	struct color color;
	color.r = (rgb >> 16) & 0xff;
	color.g = (rgb >> 8) & 0xff;
	color.b = (rgb >> 0) & 0xff;
	return color;
}

/**
 * Draw a pixel at the specified location, in the specified color. For bulk
 * rendering, such as drawing a rectangle or other polygon, the specialized
 * functions are, by design, far faster.
 * @param ctx The render context to draw to
 * @param x Pixel x coordinate
 * @param y Pixel y coordinate
 * @param color Pixel color
 */

void graphics_plot_pixel(struct render_context *ctx, uint32_t x, uint32_t y,
			 uint32_t vga_color)
{
	ctx->framebuffer[y * (ctx->pitch / sizeof(uint32_t)) + x] = vga_color;
	
}



void graphics_init(void)
{
	
	global_rctx.framebuffer = (uint8_t *) fb_tag->framebuffer_addr;
	global_rctx.width = fb_tag->framebuffer_width;
	global_rctx.height = fb_tag->framebuffer_height;
	global_rctx.pixel_width = fb_tag->framebuffer_bpp / 8;
	global_rctx.pitch = fb_tag->framebuffer_pitch;
	global_rctx.framebuffer_size = (global_rctx.height * global_rctx.pitch) +
	                                (global_rctx.width * global_rctx.pixel_width);
									
}