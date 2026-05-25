#include "graphics/gfx.h"
#include "fonts/noto_sans.h"
#include "fonts/dos_vga_437.h"

void gfx_draw_pixel(int32_t x, int32_t y, uint32_t r, uint32_t g, uint32_t b)
{

    if (x < 0 || y < 0 || x >= vesa_video_info.width || y >= vesa_video_info.height) {
        return;
    }

    uint32_t color = (r << 16) | (g << 8) | b;

    // Write the pixel to the framebuffer
    ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;
    
}

void gfx_draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t r, uint32_t g, uint32_t b) {

    if ((x1 < 0 && x2 < 0) || (x1 >= vesa_video_info.width && x2 >= vesa_video_info.width) ||
        (y1 < 0 && y2 < 0) || (y1 >= vesa_video_info.height && y2 >= vesa_video_info.height)) {
        return;
    }

    // Ensure that color is properly packed in RGB format
    uint32_t color = (r << 16) | (g << 8) | b;

    int x,y,dx,dy,dx1,dy1,px,py,xe,ye,i;
    dx=x2-x1;
    dy=y2-y1;
    dx1=abs(dx);
    dy1=abs(dy);
    px=2*dy1-dx1;
    py=2*dx1-dy1;
    if(dy1<=dx1){
        if(dx>=0){
            x=x1;
            y=y1;
            xe=x2;
        }
        else{
            x=x2;
            y=y2;
            xe=x1;
        }
        ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;

        for(i=0;x<xe;i++){
            x=x+1;
            if(px<0)
            {
                px=px+2*dy1;
            }

            else{
                if((dx<0 && dy<0) || (dx>0 && dy>0)){
                    y=y+1;
                }
                else{
                    y=y-1;
                }
                px=px+2*(dy1-dx1);
            }
            ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;
        }
    }
    else{
        if(dy>=0)
        {
            x=x1;
            y=y1;
            ye=y2;
        }
        else{
            x=x2;
            y=y2;
            ye=y1;
        }
        ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;
        
        for(i=0;y<ye;i++){
            y=y+1;
            if(py<=0){
                py=py+2*dx1;
            }
            else
            {
                if((dx<0 && dy<0) || (dx>0 && dy>0)){
                    x=x+1;
                }
                else{
                    x=x-1;
                }
                py=py+2*(dx1-dy1);
            }
            ((uint32_t*)vesa_video_info.virt_addr)[y * vesa_video_info.width + x] = color;
        }
    }
}

void gfx_draw_rectangle(int32_t x, int32_t y, int32_t width, int32_t height,  uint32_t r, uint32_t g, uint32_t b)
{
    if (x + width < 0 || x >= vesa_video_info.width || 
        y + height < 0 || y >= vesa_video_info.height) {
        return;
    }

    // Draw the top edge
    gfx_draw_line(x, y, x + width, y, r, g, b);

    // Draw the bottom edge
    gfx_draw_line(x, y + height, x + width, y + height, r, g, b);

    // Draw the left edge
    gfx_draw_line(x, y, x, y + height, r, g, b);

    // Draw the right edge
    gfx_draw_line(x + width, y, x + width, y + height, r, g, b);
}

void gfx_draw_filled_rectangle(int32_t x, int32_t y, int32_t width, int32_t height,  uint32_t r, uint32_t g, uint32_t b)
{
    int32_t x0 = x < 0 ? 0 : x;
    int32_t y0 = y < 0 ? 0 : y;
    int32_t x1 = x + width  > vesa_video_info.width  ? vesa_video_info.width  : x + width;
    int32_t y1 = y + height > vesa_video_info.height ? vesa_video_info.height : y + height;

    if (x0 >= x1 || y0 >= y1) return;

    uint32_t color = (r << 16) | (g << 8) | b;

    for (int32_t y_ = y0; y_ < y1; y_++) {
        for (int32_t x_ = x0; x_ < x1; x_++) {
            ((uint32_t*)vesa_video_info.virt_addr)[y_ * vesa_video_info.width + x_] = color;
        }
    }
}

void gfx_draw_circle(int32_t centerX, int32_t centerY, int32_t radius, uint32_t r, uint32_t g, uint32_t b)
{
    if (centerX - radius < 0 || centerX + radius > vesa_video_info.width ||
        centerY - radius < 0 || centerY + radius > vesa_video_info.height) {
        return;
    }
            
    uint32_t color = (r << 16) | (g << 8) | b;

    int l;
    // Calculate the maximum x distance from the center
    l = (int) radius * COS(PI / 4);

    uint32_t* framebuffer_virt_addr = ((uint32_t*)vesa_video_info.virt_addr);

    for (int x = 0; x <= l; x++) {
        // Calculate y based on the Pythagorean theorem for the circle
        int y = (int) sqrt((double) (radius * radius) - (x * x));

        // Draw the four symmetric points of the circle relative to the center (centerX, centerY)
        framebuffer_virt_addr[(centerY + y) * vesa_video_info.width + (centerX + x)] = color;
        framebuffer_virt_addr[(centerY - y) * vesa_video_info.width + (centerX + x)] = color;
        framebuffer_virt_addr[(centerY + y) * vesa_video_info.width + (centerX - x)] = color;
        framebuffer_virt_addr[(centerY - y) * vesa_video_info.width + (centerX - x)] = color;

        framebuffer_virt_addr[(centerY + x) * vesa_video_info.width + (centerX + y)] = color;
        framebuffer_virt_addr[(centerY - x) * vesa_video_info.width + (centerX + y)] = color;
        framebuffer_virt_addr[(centerY + x) * vesa_video_info.width + (centerX - y)] = color;
        framebuffer_virt_addr[(centerY - x) * vesa_video_info.width + (centerX - y)] = color;
    }
}

void gfx_draw_filled_circle(int32_t centerX, int32_t centerY, int32_t radius, uint32_t r, uint32_t g, uint32_t b)
{
    int x, y;

    if (centerX - radius < 0 || centerX + radius > vesa_video_info.width ||
        centerY - radius < 0 || centerY + radius > vesa_video_info.height) {
        return;
    }

    uint32_t color = (r << 16) | (g << 8) | b;

    uint32_t* framebuffer_virt_addr = ((uint32_t*)vesa_video_info.virt_addr);

    // Iterate through all possible (x, y) positions within the bounding box of the circle
    for (y = -radius; y <= radius; y++) {
        for (x = -radius; x <= radius; x++) {
            // Check if the current point (x, y) is inside or on the circle using the equation (x^2 + y^2 <= r^2)
            if ((x * x) + (y * y) <= (radius * radius)) {
                // Set pixel at (centerX + x, centerY + y) to the color
                framebuffer_virt_addr[(centerY + y) * vesa_video_info.width + (centerX + x)] = color;
            }
        }
    }
}

// Helpers
static inline uint8_t gfx_unpack_pixel(
    uint8_t byte,
    uint32_t index)
{
#if FONT_BPP == 1

    return (byte >> (7 - index)) & 0x01;

#elif FONT_BPP == 2

    return (byte >> ((3 - index) * 2)) & 0x03;

#elif FONT_BPP == 4

    return (byte >> ((1 - index) * 4)) & 0x0F;

#elif FONT_BPP == 8

    (void)index;
    return byte;

#else
#error Unsupported FONT_BPP
#endif
}

static inline uint8_t gfx_expand_alpha(
    uint8_t value)
{
#if FONT_BPP == 1

    return value ? 255 : 0;

#elif FONT_BPP == 2

    return value * 85;

#elif FONT_BPP == 4

    return value * 17;

#elif FONT_BPP == 8

    return value;

#else
#error Unsupported FONT_BPP
#endif
}

// Alpha Blend
static inline uint32_t gfx_blend(
    uint32_t dst,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t alpha)
{
    if (alpha == 255)
    {
        return (r << 16) |
               (g << 8) |
               b;
    }

    uint8_t dr = (dst >> 16) & 0xFF;
    uint8_t dg = (dst >> 8) & 0xFF;
    uint8_t db = dst & 0xFF;

    uint32_t nr =
        ((r * alpha) +
        (dr * (255 - alpha))) / 255;

    uint32_t ng =
        ((g * alpha) +
        (dg * (255 - alpha))) / 255;

    uint32_t nb =
        ((b * alpha) +
        (db * (255 - alpha))) / 255;

    return (nr << 16) |
           (ng << 8) |
           nb;
}

// Draw Character
int32_t gfx_draw_char(int32_t x, int32_t y, char c, uint32_t r, uint32_t g, uint32_t b)
{
    // Validate character range
    if ((uint8_t)c < FONT_FIRST ||
        (uint8_t)c > FONT_LAST)
    {
        return FONT_HEIGHT / 2;
    }

    // Lookup glyph
    uint32_t glyph_index =
        (uint8_t)c - FONT_FIRST;

    #if FONT_USE_DOS_VGA
        const Glyph *glyphs =
            &dos_vga_437Glyphs[glyph_index];
        const uint8_t *glyph =
            &dos_vga_437[glyphs->offset];
    #else
        const Glyph *glyphs =
            &NotoSansGlyphs[glyph_index];
        const uint8_t *glyph =
            &noto_sans[glyphs->offset];
    #endif

    const uint32_t glyph_width =
        glyphs->width;

    // Packed bitmap geometry
    const uint32_t pixels_per_byte =
        FONT_PIXELS_PER_BYTE;

    const uint32_t bytes_per_row =
        (glyph_width + pixels_per_byte - 1) /
        pixels_per_byte;

    // Framebuffer
    uint32_t *fb =
        (uint32_t *)vesa_video_info.virt_addr;

    // Render glyph
    for (uint32_t row = 0;
         row < FONT_HEIGHT;
         row++)
    {
        const uint32_t row_offset =
            row * bytes_per_row;

        for (uint32_t col = 0;
             col < glyph_width;
             col++)
        {
            // Packed byte lookup
            uint32_t byte_index =
                row_offset +
                (col / pixels_per_byte);

            uint8_t packed =
                glyph[byte_index];

            // Pixel inside packed byte
            uint32_t pixel_index =
                col % pixels_per_byte;

            uint8_t pixel =
                gfx_unpack_pixel(
                    packed,
                    pixel_index);

            // transparent pixel
            if (pixel == 0)
            {
                continue;
            }

            // Convert BPP value -> 0..255 alpha
            uint8_t alpha =
                gfx_expand_alpha(pixel);

            // Screen position
            int32_t px =
                x + (int32_t)col;

            int32_t py =
                y + (int32_t)row;

            // Clipping
            if (px < 0 ||
                py < 0 ||
                px >= (int32_t)vesa_video_info.width ||
                py >= (int32_t)vesa_video_info.height)
            {
                continue;
            }

            // Destination framebuffer pixel
            uint32_t *dst =
                &fb[
                    py * vesa_video_info.width +
                    px
                ];

            // Blend glyph pixel
            *dst = gfx_blend(
                *dst,
                r,
                g,
                b,
                alpha);
        }
    }

    // Advance cursor
    return (int32_t)(
        glyph_width +
        FONT_SPACING
    );
}

// Draw Text
void gfx_draw_text(
    int32_t x,
    int32_t y,
    const char *str,
    uint32_t r,
    uint32_t g,
    uint32_t b)
{
    int32_t cx = x;

    while (*str)
    {
        if (*str == '\n')
        {
            cx = x;

            y += FONT_HEIGHT;

            str++;

            continue;
        }

        cx += gfx_draw_char(
            cx,
            y,
            *str,
            r,
            g,
            b);

        str++;
    }
}

// Text Width
int32_t gfx_text_width(
    const char *str)
{
    int32_t width = 0;

    int32_t line_width = 0;

    while (*str)
    {
        if (*str == '\n')
        {
            if (line_width > width)
            {
                width = line_width;
            }

            line_width = 0;

            str++;

            continue;
        }

        if ((uint8_t)*str >= FONT_FIRST &&
            (uint8_t)*str <= FONT_LAST)
        {
            uint32_t glyph_index =
                (uint8_t)*str - FONT_FIRST;

            #if FONT_USE_DOS_VGA
                line_width +=
                    dos_vga_437Glyphs[glyph_index].width +
                    FONT_SPACING;
            #else
                line_width +=
                    NotoSansGlyphs[glyph_index].width +
                    FONT_SPACING;
            #endif

            
        }

        str++;
    }

    if (line_width > width)
    {
        width = line_width;
    }

    return width;
}