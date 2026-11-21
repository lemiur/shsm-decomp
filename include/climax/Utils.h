#ifndef CLIMAX_UTILS_H
#define CLIMAX_UTILS_H

// Header name invented (see src/climax/Utils.cpp).

#include <revolution/types.h>
#include <stddef.h>
#include <climax/Clump.h>
#include <rwsdk/batextur.h>

/* RpHAnimHierarchy (RenderWare 3.7 RpHAnim plugin): only handled through pointers here. */
struct unk_RpHAnimHierarchy;

/*
 * A node of a Climax XML document (text or binary); only the member the reconstructed code reads
 * is known, the rest of the layout is not. Name invented.
 */
struct unk_XmlNode {
    u8              field_0x0[8];   // unknown
    char*           text;           // 0x8: the node's text, by its use (a value read as a string)
};

extern "C" {

// defined in 0x802C4E64 (RenderWare), unit not yet reconstructed: the animation hierarchy
// attached to frame, or NULL (presumed RpHAnimFrameGetHierarchy, by its use here)
unk_RpHAnimHierarchy* RpHAnimFrameGetHierarchy( unk_RwFrame* frame );

// defined in 0x802EAA14 (RenderWare), unit not yet reconstructed: attaches hierarchy to a skinned
// atomic (presumed RpSkinAtomicSetHAnimHierarchy, by its use here)
unk_RpAtomic* RpSkinAtomicSetHAnimHierarchy( unk_RpAtomic* atomic, unk_RpHAnimHierarchy* hierarchy );

}

// defined in 0x801B3538 (Climax file layer), unit not yet reconstructed: nonzero when the file
// called name exists (by its use here; the meaning of the other arguments is unknown)
int fn_801B3538( const char* name, int arg1, int arg2, int arg3 );

// defined in 0x801B3C40 (Climax file layer), unit not yet reconstructed: the contents of the
// file called name in a new[] array, and their size in *size; NULL when it cannot be read
u8* fn_801B3C40( const char* name, u32* size, int flags );

// defined in 0x801CCB98 (Climax XML), unit not yet reconstructed: the child of node called name
// (compared without case), or NULL
unk_XmlNode* fn_801CCB98( unk_XmlNode* node, const char* name );

// defined in 0x801CDEA8 (Climax XML), unit not yet reconstructed: parses a text or binary
// ("FBX") document held in text and returns its root, or NULL on a parse error; a binary
// document is used in place, so its root is text itself
unk_XmlNode* fn_801CDEA8( char* text );

extern "C" int       fn_801B6138( const char* a, const char* b );  // true when the strings are equal, ignoring case
extern "C" int       fn_801B6188( const char* a, const char* b, int n );  // true when the first n characters are equal, ignoring case
char*                   fn_801B67FC( const char* str, int extra );
char*                   fn_801B6870( const char* str, int extra );
void                    fn_801B68E4( char* dst, const char* src, int size );
char*                   fn_801B68F4( const wchar_t* src, char* dst );
wchar_t*                fn_801B6994( const char* src, wchar_t* dst );
int                     fn_801B6A30( const char* str );
unk_RpHAnimHierarchy*   fn_801B6AD8( unk_RpClump* clump );
void                    fn_801B6B44( unk_RpClump* clump, unk_RpHAnimHierarchy* hierarchy );
RwTexture*              fn_801B6C00( const char* name, int addRef );
u32                     fn_801B6C64( u32 value, u32 alignment );
u32                     fn_801B6C78( u32 value );
s16                     fn_801B6C98( s16 value );
float                   fn_801B6CAC( float value );
void                    fn_801B6CE4( const char** str );
int                     fn_801B6D08( unk_XmlNode* const* node, const char* name, unk_XmlNode** child );
int                     fn_801B6D50( unk_XmlNode** root, const char* fileName );

#endif
