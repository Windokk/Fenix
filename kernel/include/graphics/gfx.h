#ifndef __GFX_H
#define __GFX_H

#include "common.h"

#include "drivers/vesa.h"

// Draw a single pixel
void gfx_draw_pixel(int32_t x, int32_t y,  uint32_t r, uint32_t g, uint32_t b);

// Draw a horizontal or vertical line
void gfx_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2,  uint32_t r, uint32_t g, uint32_t b);

// Draw a rectangle
void gfx_draw_rectangle(int32_t x, int32_t y, int32_t width, int32_t height,  uint32_t r, uint32_t g, uint32_t b);

// Draw a filled rectangle
void gfx_draw_filled_rectangle(int32_t x, int32_t y, int32_t width, int32_t height,  uint32_t r, uint32_t g, uint32_t b);

// Draw a circle
void gfx_draw_circle(int32_t centerX, int32_t centerY, int32_t radius, uint32_t r, uint32_t g, uint32_t b);

// Draw a filled circle
void gfx_draw_filled_circle(int32_t x, int32_t y, int32_t radius,  uint32_t r, uint32_t g, uint32_t b);

#endif