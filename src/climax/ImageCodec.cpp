// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md. The unit holds the YUVQ image
// codec: 32-bit images compressed to 8x8 blocks of 4-bit YCbCr steps (BT.601 coefficients).
// Its .sdata2 (0x8052E440-0x8052E490) holds only the float literals of the two conversions.

#include <climax/ImageCodec.h>
#include <rwsdk/baimage.h>
#include <rwsdk/plcore/babinary.h>

/*
====================
SwapHeader

Swaps the 16-bit fields of header between stored and native byte order. Name invented.
====================
*/
static inline void SwapHeader( unk_YUVQHeader* header ) {
    RwMemNative16( &header->width, sizeof( header->width ) );
    RwMemNative16( &header->height, sizeof( header->height ) );
    RwMemNative16( &header->blocksWide, sizeof( header->blocksWide ) );
    RwMemNative16( &header->blocksHigh, sizeof( header->blocksHigh ) );
}

/*
====================
fn_801B6E44

The size of the compressed form of a width x height image: a block of 54 bytes per 8x8 pixels
and a 12-byte header, rounded up to 32 bytes.
====================
*/
// 0x801B6E44
int fn_801B6E44( int width, int height ) {
    return ( ( width + 7 ) / 8 * 54 * ( ( height + 7 ) / 8 ) + 12 + 31 ) & ~31;
}

/*
====================
fn_801B6E70

The size of the compressed form of image, or 0 when it is missing or not 32 bits per pixel.
====================
*/
// 0x801B6E70
int fn_801B6E70( const RwImage* image ) {
    if ( image == NULL ) {
        return 0;
    }
    if ( image->depth != 32 ) {
        return 0;
    }
    return fn_801B6E44( image->width, image->height );
}

/*
====================
fn_801B6EC8

Compresses the 32-bit image into buffer (fn_801B6E70 bytes) as a YUVQ image. Returns false
when either is missing or the image is not 32 bits per pixel.
====================
*/
// 0x801B6EC8
bool fn_801B6EC8( RwImage* image, void* buffer ) {
    unk_YUVQHeader* header = static_cast<unk_YUVQHeader*>( buffer );

    if ( image == NULL ) {
        return false;
    }
    if ( header == NULL ) {
        return false;
    }
    if ( image->depth != 32 ) {
        return false;
    }

    header->magic[0] = 'Y';
    header->magic[1] = 'U';
    header->magic[2] = 'V';
    header->magic[3] = 'Q';
    header->width = image->width;
    header->height = image->height;
    header->blocksWide = ( header->width + 7 ) / 8;
    header->blocksHigh = ( header->height + 7 ) / 8;

    unk_YUVQBlock* block = header->blocks;
    for ( int blockY = 0; blockY < header->blocksHigh; blockY++ ) {
        for ( int blockX = 0; blockX < header->blocksWide; blockX++ ) {
            int lum[64];
            int cb[64];
            int cr[64];
            int cbAvg[16];
            int crAvg[16];
            int i;
            const u8* pixel;

            // convert the block to YCbCr, repeating the last row past the bottom edge
            i = 0;
            for ( int row = 0; row < 8; row++ ) {
                int pixelY = blockY * 8 + row;
                if ( pixelY >= image->height ) {
                    pixelY = image->height - 1;
                }
                const u8* line = image->cpPixels + pixelY * image->stride;
                for ( int col = 0; col < 8; col++ ) {
                    int pixelX = blockX * 8 + col;
                    // BUG: one pixel past the right edge, not the last pixel of the row
                    if ( pixelX >= image->width ) {
                        pixelX = image->width;
                    }
                    pixel = &line[pixelX * 4];
                    float red = pixel[0];
                    float green = pixel[1];
                    float blue = pixel[2];

                    int luma = 0.257f * red + 0.504f * green + 0.098f * blue + 16.0f;
                    int blueDiff = -( 0.148f * red ) - 0.291f * green + 0.439f * blue + 128.0f;
                    int redDiff = 0.439f * red - 0.368f * green - 0.071f * blue + 128.0f;
                    if ( luma < 0 ) {
                        luma = 0;
                    }
                    if ( luma > 255 ) {
                        luma = 255;
                    }
                    if ( blueDiff < 0 ) {
                        blueDiff = 0;
                    }
                    if ( blueDiff > 255 ) {
                        blueDiff = 255;
                    }
                    if ( redDiff < 0 ) {
                        redDiff = 0;
                    }
                    if ( redDiff > 255 ) {
                        redDiff = 255;
                    }
                    lum[i] = luma;
                    cb[i] = blueDiff;
                    cr[i] = redDiff;
                    i++;
                }
            }

            // average the chroma over 2x2 pixels
            i = 0;
            for ( int row = 0; row < 4; row++ ) {
                for ( int col = 0; col < 4; col++ ) {
                    int k = row * 16 + col * 2;
                    // BUG: the top right sample is counted twice and the bottom left one not at all
                    cbAvg[i] = ( cb[k] + cb[k + 1] + cb[k + 1] + cb[k + 9] ) / 4;
                    crAvg[i] = ( cr[k] + cr[k + 1] + cr[k + 1] + cr[k + 9] ) / 4;
                    i++;
                }
            }

            // the range of each channel, at least 15 so that every step is at least 1
            int yMax = block->yMin = lum[0];
            int uMax = block->uMin = cbAvg[0];
            int vMax = block->vMin = crAvg[0];
            for ( i = 1; i < 64; i++ ) {
                if ( lum[i] > yMax ) {
                    yMax = lum[i];
                }
                if ( lum[i] < block->yMin ) {
                    block->yMin = lum[i];
                }
            }
            for ( i = 1; i < 16; i++ ) {
                if ( cbAvg[i] > uMax ) {
                    uMax = cbAvg[i];
                }
                if ( cbAvg[i] < block->uMin ) {
                    block->uMin = cbAvg[i];
                }
                if ( crAvg[i] > vMax ) {
                    vMax = crAvg[i];
                }
                if ( crAvg[i] < block->vMin ) {
                    block->vMin = crAvg[i];
                }
            }
            block->yRange = yMax - block->yMin;
            block->uRange = uMax - block->uMin;
            block->vRange = vMax - block->vMin;
            if ( block->yRange < 15 ) {
                block->yRange = 15;
            }
            if ( block->uRange < 15 ) {
                block->uRange = 15;
            }
            if ( block->vRange < 15 ) {
                block->vRange = 15;
            }

            // quantise to 4-bit steps, two per byte
            for ( i = 0; i < 64; i += 2 ) {
                block->y[i / 2] = ( lum[i] - block->yMin ) * 15 / block->yRange
                                | ( lum[i + 1] - block->yMin ) * 15 / block->yRange << 4;
            }
            for ( i = 0; i < 16; i += 2 ) {
                block->u[i / 2] = ( cbAvg[i] - block->uMin ) * 15 / block->uRange
                                | ( cbAvg[i + 1] - block->uMin ) * 15 / block->uRange << 4;
            }
            for ( i = 0; i < 16; i += 2 ) {
                block->v[i / 2] = ( crAvg[i] - block->vMin ) * 15 / block->vRange
                                | ( crAvg[i + 1] - block->vMin ) * 15 / block->vRange << 4;
            }
            block++;
        }
    }

    SwapHeader( header );
    return true;
}

/*
====================
fn_801B7B9C

A new 32-bit image decoded from the YUVQ image at data, or NULL when data is missing, not a
YUVQ image, or the image cannot be made. The header is swapped to native byte order in place
and stays so when the image is made.
====================
*/
// 0x801B7B9C
RwImage* fn_801B7B9C( void* data ) {
    unk_YUVQHeader* header = static_cast<unk_YUVQHeader*>( data );

    if ( header == NULL ) {
        return NULL;
    }
    const unk_YUVQBlock* block = header->blocks;
    if ( header->magic[0] != 'Y' || header->magic[1] != 'U' || header->magic[2] != 'V'
      || header->magic[3] != 'Q' ) {
        return NULL;
    }

    SwapHeader( header );
    RwImage* image = RwImageCreate( header->width, header->height, 32 );
    if ( image == NULL ) {
        SwapHeader( header );
        return NULL;
    }
    if ( RwImageAllocatePixels( image ) == NULL ) {
        // BUG: the image is leaked
        SwapHeader( header );
        return NULL;
    }

    for ( int blockY = 0; blockY < header->blocksHigh; blockY++ ) {
        for ( int blockX = 0; blockX < header->blocksWide; blockX++ ) {
            int lum[64];
            int cb[16];
            int cr[16];
            int i;

            // expand the 4-bit steps
            for ( i = 0; i < 64; i += 2 ) {
                lum[i] = block->yMin + block->yRange * ( block->y[i / 2] & 0xF ) / 15;
                lum[i + 1] = block->yMin + block->yRange * ( block->y[i / 2] >> 4 ) / 15;
            }
            for ( i = 0; i < 16; i += 2 ) {
                cb[i] = block->uMin + block->uRange * ( block->u[i / 2] & 0xF ) / 15;
                cb[i + 1] = block->uMin + block->uRange * ( block->u[i / 2] >> 4 ) / 15;
            }
            for ( i = 0; i < 16; i += 2 ) {
                cr[i] = block->vMin + block->vRange * ( block->v[i / 2] & 0xF ) / 15;
                cr[i + 1] = block->vMin + block->vRange * ( block->v[i / 2] >> 4 ) / 15;
            }

            // convert to RGB, leaving alpha unset and skipping pixels past the image's edges
            for ( int row = 0; row < 8; row++ ) {
                int pixelY = blockY * 8 + row;
                if ( pixelY < image->height ) {
                    u8* line = image->cpPixels + pixelY * image->stride;
                    for ( int col = 0; col < 8; col++ ) {
                        int pixelX = blockX * 8 + col;
                        if ( pixelX < image->width ) {
                            float y = lum[row * 8 + col];
                            float u = cb[row / 2 * 4 + col / 2];
                            float v = cr[row / 2 * 4 + col / 2];
                            int red = 1.164f * ( y - 16.0f ) + 1.596f * ( v - 128.0f );
                            int green = 1.164f * ( y - 16.0f ) - 0.813f * ( v - 128.0f )
                                      - 0.391f * ( u - 128.0f );
                            int blue = 1.164f * ( y - 16.0f ) + 2.018f * ( u - 128.0f );
                            if ( red < 0 ) {
                                red = 0;
                            }
                            if ( red > 255 ) {
                                red = 255;
                            }
                            if ( green < 0 ) {
                                green = 0;
                            }
                            if ( green > 255 ) {
                                green = 255;
                            }
                            if ( blue < 0 ) {
                                blue = 0;
                            }
                            if ( blue > 255 ) {
                                blue = 255;
                            }
                            line[pixelX * 4] = red;
                            line[pixelX * 4 + 1] = green;
                            line[pixelX * 4 + 2] = blue;
                        }
                    }
                }
            }
            block++;
        }
    }
    return image;
}
