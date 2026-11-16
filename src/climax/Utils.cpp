// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md. The unit holds the engine's
// general helpers: string copies, RenderWare hierarchy and texture searches, byte swaps and an
// XML file loader.

#include <climax/Utils.h>
#include <climax/Memory.h>
#include <extras.h>
#include <string.h>
#include <wchar.h>
#include <rwsdk/badevice.h>

/*
====================
fn_801B67FC

A new copy of str with extra spare bytes, or NULL for a NULL or empty result.
====================
*/
// 0x801B67FC
char* fn_801B67FC( const char* str, int extra ) {
    return fn_801B6870( str, extra );
}

/*
====================
fn_801B6870

A new copy of str with extra spare bytes after its terminator, or NULL when str is NULL or the
copy would be empty.
====================
*/
// 0x801B6870
char* fn_801B6870( const char* str, int extra ) {
    char* copy = NULL;

    if ( str != NULL ) {
        size_t length = strlen( str ) + extra;
        if ( length != 0 ) {
            copy = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[length + 1];
            strcpy( copy, str );
        }
    }
    return copy;
}

/*
====================
fn_801B68E4

Copies src into the size bytes of dst.
====================
*/
// 0x801B68E4
void fn_801B68E4( char* dst, const char* src, int size ) {
    // the original's bug: the terminator is written before strncpy, which overwrites it when src
    // is size characters or longer
    dst[size - 1] = '\0';
    strncpy( dst, src, size );
}

/*
====================
fn_801B68F4

Narrows the wide string src into dst (a new array when dst is NULL), dropping the control
characters up to tab; returns dst, or NULL when src is NULL.
====================
*/
// 0x801B68F4
char* fn_801B68F4( const wchar_t* src, char* dst ) {
    char* result = NULL;

    if ( src != NULL ) {
        size_t length = wcslen( src );
        if ( dst == NULL ) {
            dst = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[length + 1];
        }

        char* out = dst;
        while ( *src != 0 ) {
            u8 c = *src++;
            if ( c > 9U ) {     // drops the control characters up to tab
                *out++ = c;
            }
        }
        *out = '\0';
        result = dst;
    }
    return result;
}

/*
====================
fn_801B6994

Widens src into dst (a new array when dst is NULL); returns dst, or NULL when src is NULL.
====================
*/
// 0x801B6994
wchar_t* fn_801B6994( const char* src, wchar_t* dst ) {
    wchar_t* result = NULL;

    if ( src != NULL ) {
        size_t length = strlen( src );
        if ( dst == NULL ) {
            dst = new( GAME_HEAP, GAME_HEAP_FLAGS ) wchar_t[length + 1];
        }

        wchar_t* out = dst;
        while ( *src != '\0' ) {
            *out++ = *src++;
        }
        *out = 0;
        result = dst;
    }
    return result;
}

/*
====================
fn_801B6A30

True when every character of str is a decimal digit (and for an empty string).
====================
*/
// 0x801B6A30
int fn_801B6A30( const char* str ) {
    int isNumber = TRUE;

    while ( *str != '\0' && isNumber ) {
        isNumber = FALSE;
        if ( *str >= '0' && *str <= '9' ) {
            isNumber = TRUE;
        }
        str++;
    }
    return isNumber;
}

/*
====================
fn_801B6A74

Frame callback: stores the first animation hierarchy found below frame in *data (a
unk_RpHAnimHierarchy*), searching depth first.
====================
*/
// 0x801B6A74
static unk_RwFrame* fn_801B6A74( unk_RwFrame* frame, void* data ) {
    unk_RpHAnimHierarchy* hierarchy = RpHAnimFrameGetHierarchy( frame );

    if ( hierarchy == NULL ) {
        RwFrameForAllChildren( frame, fn_801B6A74, data );
        return frame;
    }
    *( unk_RpHAnimHierarchy** )data = hierarchy;
    return NULL;
}

/*
====================
fn_801B6AD8

The animation hierarchy of clump's frame tree, or NULL.
====================
*/
// 0x801B6AD8
unk_RpHAnimHierarchy* fn_801B6AD8( unk_RpClump* clump ) {
    unk_RpHAnimHierarchy* hierarchy = NULL;

    RwFrameForAllChildren( RpClumpGetFrame( clump ), fn_801B6A74, &hierarchy );
    return hierarchy;
}

/*
====================
fn_801B6B14

Atomic callback: attaches the hierarchy data to a skinned atomic.
====================
*/
// 0x801B6B14
static unk_RpAtomic* fn_801B6B14( unk_RpAtomic* atomic, void* data ) {
    RpSkinAtomicSetHAnimHierarchy( atomic, ( unk_RpHAnimHierarchy* )data );
    return atomic;
}

/*
====================
fn_801B6B44

Attaches hierarchy to every skinned atomic of clump.
====================
*/
// 0x801B6B44
void fn_801B6B44( unk_RpClump* clump, unk_RpHAnimHierarchy* hierarchy ) {
    RpClumpForAllAtomics( clump, fn_801B6B14, hierarchy );
}

/* The state of a texture search by name. Name invented. */
struct unk_TextureSearch {
    const char*     name;       // 0x0
    RwTexture*      texture;    // 0x4, the texture found
};

/*
====================
fn_801B6B54

Texture callback: stops at the texture whose name matches the search (without case).
====================
*/
// 0x801B6B54
static RwTexture* fn_801B6B54( RwTexture* texture, void* data ) {
    unk_TextureSearch* search = ( unk_TextureSearch* )data;
    RwTexture* result = texture;

    if ( stricmp( texture->name, search->name ) == 0 ) {
        search->texture = texture;
        result = NULL;
    }
    return result;
}

/*
====================
fn_801B6BA8

Texture dictionary callback: searches dict's textures, stopping once the texture is found.
====================
*/
// 0x801B6BA8
static RwTexDictionary* fn_801B6BA8( RwTexDictionary* dict, void* data ) {
    unk_TextureSearch* search = ( unk_TextureSearch* )data;

    RwTexDictionary* result = dict;

    RwTexDictionaryForAllTextures( dict, fn_801B6B54, search );
    if ( search->texture != NULL ) {
        result = NULL;
    }
    return result;
}

/*
====================
fn_801B6C00

The texture called name in any texture dictionary, or NULL; adds a reference to it when addRef
is set.
====================
*/
// 0x801B6C00
RwTexture* fn_801B6C00( const char* name, int addRef ) {
    unk_TextureSearch search;

    search.name = name;
    search.texture = NULL;
    RwTexDictionaryForAllTexDictionaries( fn_801B6BA8, &search );

    RwTexture* texture = search.texture;
    if ( texture != NULL && addRef ) {
        texture->refCount++;
    }
    return texture;
}

/*
====================
fn_801B6C64

value rounded up to a multiple of alignment (a power of two).
====================
*/
// 0x801B6C64
u32 fn_801B6C64( u32 value, u32 alignment ) {
    return ( value + alignment - 1 ) & ~( alignment - 1 );
}

/*
====================
fn_801B6C78

value with its byte order reversed.
====================
*/
// 0x801B6C78
u32 fn_801B6C78( u32 value ) {
    return ( value >> 24 ) + ( ( value >> 8 ) & 0xFF00 ) + ( ( value << 24 ) + ( ( value << 8 ) & 0xFF0000 ) );
}

/*
====================
fn_801B6C98

value with its byte order reversed.
====================
*/
// 0x801B6C98
s16 fn_801B6C98( s16 value ) {
    return ( ( value & 0xFF ) << 8 ) + ( ( value >> 8 ) & 0xFF );
}

/*
====================
fn_801B6CAC

value with the byte order of its bits reversed.
====================
*/
// 0x801B6CAC
float fn_801B6CAC( float value ) {
    u32 swapped = fn_801B6C78( *( u32* )&value );
    return *( float* )&swapped;
}

/*
====================
fn_801B6CE4

Clears *str when it points to an empty string.
====================
*/
// 0x801B6CE4
void fn_801B6CE4( const char** str ) {
    if ( *str != NULL && **str == '\0' ) {
        *str = NULL;
    }
}

/*
====================
fn_801B6D08

Finds the child of *node called name; true, with the child in *child, when there is one.
====================
*/
// 0x801B6D08
int fn_801B6D08( unk_XmlNode* const* node, const char* name, unk_XmlNode** child ) {
    unk_XmlNode* found = fn_801CCB98( *node, name );

    if ( found == NULL ) {
        return FALSE;
    }
    *child = found;
    return TRUE;
}

/*
====================
fn_801B6D50

Loads and parses the XML document in the file called fileName; true, with its root in *root,
when it was read and parsed.
====================
*/
// 0x801B6D50
int fn_801B6D50( unk_XmlNode** root, const char* fileName ) {
    u32 size;
    u32 length;
    int bufferSize;
    int loaded = FALSE;
    u8* data;
    char* text;

    if ( fn_801B3538( fileName, 0, 0, 1 ) ) {
        size = 0;
        data = fn_801B3C40( fileName, &size, 0 );
        length = size;

        if ( data == NULL ) {
            loaded = FALSE;
        } else {
            // room for the text and its terminator, and one byte to spare
            bufferSize = length + 1;
            text = ( char* )RwMalloc( bufferSize + 1, 0 );
            text[bufferSize - 1] = '\0';
            memcpy( text, data, length );

            *root = fn_801CDEA8( text );
            if ( ( void* )*root != text ) {
                RwFree( text );
            }
            loaded = *root != NULL;
        }
        delete[] data;
    }
    return loaded;
}
