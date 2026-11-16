#ifndef CLIMAX_FONT_H
#define CLIMAX_FONT_H

#include <climax/Colour.h>

/*
 * Text drawing of the Climax engine (0x801CE59C and 0x801CF248, unit not yet reconstructed).
 * Header and type names invented.
 */

struct unk_Font;    // a loaded font

/* The box a text is drawn in, in screen units. Name invented. */
struct unk_TextBox {
    float   x;          // 0x0
    float   y;          // 0x4
    float   width;      // 0x8
    float   height;     // 0xC
};

// defined in 0x801CE59C, unit not yet reconstructed: the font called name, the first one for NULL
unk_Font*   fn_801CE59C( const char* name );
// defined in 0x801CF248, unit not yet reconstructed: draws text in box; the meaning of the three
// integer arguments is unknown
int         fn_801CF248( unk_Font* font, const char* text, const unk_TextBox* box, const unk_RGBA* colour,
                         int arg4, int arg5, int arg6, float size );

#endif
