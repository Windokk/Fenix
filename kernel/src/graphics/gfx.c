#include "graphics/gfx.h"

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
    if (x + width < 0 || x >= vesa_video_info.width || 
        y + height < 0 || y >= vesa_video_info.height) {
        return;
    }

    uint32_t color = (r << 16) | (g << 8) | b;

    for (int y_ = y; y_ < y + height; y_++) {
        for (int x_ = x; x_ < x + width; x_++) {
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
