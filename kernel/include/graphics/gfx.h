#pragma once

#include "common.h"

#include "drivers/video.h"

#define GFX_LINE_HEIGHT 24

// Draw a single pixel
void gfx_draw_pixel(uint32_t x, uint32_t y,  uint32_t r, uint32_t g, uint32_t b);

// Draw a horizontal or vertical line
void gfx_draw_line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,  uint32_t r, uint32_t g, uint32_t b);

// Draw a rectangle
void gfx_draw_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height,  uint32_t r, uint32_t g, uint32_t b);

// Draw a filled rectangle
void gfx_draw_filled_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height,  uint32_t r, uint32_t g, uint32_t b);

// Draw a circle
void gfx_draw_circle(uint32_t centerX, uint32_t centerY, uint32_t radius, uint32_t r, uint32_t g, uint32_t b);

// Draw a filled circle
void gfx_draw_filled_circle(uint32_t x, uint32_t y, uint32_t radius,  uint32_t r, uint32_t g, uint32_t b);

// Draw a single character using the built-in bitmap font (NotoSans 24pt)
// Returns the advance width in pixels.
int32_t gfx_draw_char(uint32_t x, uint32_t y, char c, uint32_t r, uint32_t g, uint32_t b);

// Draw a null-terminated string. Honors '\n' (resets x, advances y by one line).
void gfx_draw_text(uint32_t x, uint32_t y, const char *str, uint32_t r, uint32_t g, uint32_t b);

// Returns the pixel width of a string (useful for centering / layout).
int32_t gfx_text_width(const char *str);