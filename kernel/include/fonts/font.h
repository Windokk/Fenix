#pragma once

// -----------------------------------------------------------------------------
// Font selection
//
// Pick ONE font by defining exactly one of these (e.g. -DFONT_DM_MONO in
// CCFLAGS, or a #define before including this header):
//
//   FONT_DOS_VGA_437   Perfect DOS VGA 437 (7 px wide)  <- default
//   FONT_DM_MONO       DM Mono Regular     (8 px wide)
// -----------------------------------------------------------------------------

#if !defined(FONT_DOS_VGA_437) && !defined(FONT_DM_MONO)
#define FONT_DOS_VGA_437
#endif

#if defined(FONT_DOS_VGA_437) && defined(FONT_DM_MONO)
#error Only one font can be selected (FONT_DOS_VGA_437 or FONT_DM_MONO)
#endif

#if defined(FONT_DOS_VGA_437)
#include "fonts/dos_vga_437.h"
#define FONT_GLYPHS dos_vga_437Glyphs
#define FONT_BITMAP dos_vga_437
#elif defined(FONT_DM_MONO)
#include "fonts/dm_mono.h"
#define FONT_GLYPHS dm_monoGlyphs
#define FONT_BITMAP dm_mono
#endif
