#ifndef CLIMAX_COLOUR_H
#define CLIMAX_COLOUR_H

#include <revolution/types.h>

/* An 8-bit colour (RwRGBA in the RenderWare 3.7 SDK). Header and name invented. */
struct unk_RGBA {
    u8      red;        // 0x0
    u8      green;      // 0x1
    u8      blue;       // 0x2
    u8      alpha;      // 0x3
};

/* True when the two colours are the same in all four channels, compared as one word. Name invented. */
inline bool operator==( const unk_RGBA& a, const unk_RGBA& b ) {
    return *( const u32* )&a == *( const u32* )&b;
}

#endif
