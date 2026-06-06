#pragma once

typedef struct {
    unsigned char ascii;
    unsigned char width;
    unsigned short offset;
} Glyph;

// -----------------------------------------------------------------------------
// Font Metadata
// -----------------------------------------------------------------------------

#define FONT_FIRST          32
#define FONT_LAST           255
#define FONT_HEIGHT         16
#define FONT_BPP            4

// pixels packed per byte
#define FONT_PIXELS_PER_BYTE (8 / FONT_BPP)

// spacing between glyphs
#define FONT_SPACING   1