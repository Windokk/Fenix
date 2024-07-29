#pragma once

#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

struct color {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct render_context {
	uint8_t *framebuffer;
	uint32_t framebuffer_size;      // the size of the framebuffer in bytes

	uint32_t width;                 // pixel width of the framebuffer
	uint32_t height;                // pixel height of the framebuffer

	uint32_t pixel_width;           // byte size of a pixel
	uint32_t pitch;                 // byte size of a row
	bool blending;
};



struct render_context* graphics_get_global_rctx(void);

struct color graphics_color_rgb(uint8_t r, uint8_t g, uint8_t b);

struct color graphics_color(uint32_t rgb);



/**
 * Draw a pixel at the specified location, in the specified color. For bulk
 * rendering, such as drawing a rectangle or other polygon, the specialized
 * functions are, by design, far faster.
 * @param ctx The render context to draw to
 * @param x Pixel x coordinate
 * @param y Pixel y coordinate
 * @param color Pixel color
 */
void graphics_plot_pixel(struct render_context *ctx, uint32_t x,
	uint32_t y, uint32_t color);


void graphics_init(void);

#endif