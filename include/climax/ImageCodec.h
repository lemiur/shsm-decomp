#ifndef CLIMAX_IMAGECODEC_H
#define CLIMAX_IMAGECODEC_H

#include <revolution/types.h>
#include <stddef.h>

struct RwImage;

/*
 * One 8x8 pixel block of a YUVQ image: each channel is stored as a minimum, a range and 4-bit
 * steps between them (value = min + range * step / 15), luma per pixel and both chroma channels
 * per 2x2 pixels. Name invented.
 */
struct unk_YUVQBlock {
    u8      yMin;       // 0x0
    u8      yRange;     // 0x1
    u8      uMin;       // 0x2, Cb
    u8      uRange;     // 0x3
    u8      vMin;       // 0x4, Cr
    u8      vRange;     // 0x5
    u8      y[32];      // 0x6, two 4-bit steps per byte, low nibble first, rows of 8 pixels
    u8      u[8];       // 0x26, rows of 4
    u8      v[8];       // 0x2E, rows of 4
};

/*
 * The header of a YUVQ image (the compressed image format of fn_801B6EC8), followed by its
 * blocks row by row. The 16-bit fields are stored little-endian. Name invented.
 */
struct unk_YUVQHeader {
    u8              magic[4];       // 0x0, "YUVQ"
    u16             width;          // 0x4
    u16             height;         // 0x6
    u16             blocksWide;     // 0x8
    u16             blocksHigh;     // 0xA
    unk_YUVQBlock   blocks[1];      // 0xC, blocksWide * blocksHigh of them
};

int             fn_801B6E44( int width, int height );
int             fn_801B6E70( const RwImage* image );
bool            fn_801B6EC8( RwImage* image, void* buffer );
RwImage*        fn_801B7B9C( void* data );

#endif
