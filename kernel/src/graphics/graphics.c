#include "graphics/graphics.h"
#include "drivers/vga.h"

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
	
	
}



void graphics_init(void)
{
	
	
									
}