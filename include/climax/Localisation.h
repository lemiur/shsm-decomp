#ifndef CLIMAX_LOCALISATION_H
#define CLIMAX_LOCALISATION_H

// unit name unconfirmed: header and type names invented (see src/climax/Localisation.cpp).

#include <revolution/types.h>
#include <climax/Utils.h>
#include <climax/unk_FileLoadCallback.h>
#include <climax/unk_List.h>

#define LANGUAGE_COUNT  6
#define REGION_COUNT    3

/* A language the game can show: its code, the font set it needs and whether it is offered. Names invented. */
struct unk_Language {
    const char*     name;           // 0x0: "Eng", "Fre", "Ita", "Ger", "Spa", "Jap"; also the suffix of its files
    const char*     font;           // 0x4: "FontEUR" or "FontJAP"
    u8              japanese;       // 0x8: set for the font set with double-byte characters
    u8              enabled;        // 0x9: named by the Locale entry of Config.xml (English is always on)
    u8              pad[2];         // pad: struct alignment
};

/* A region the game was released in, by its name in Config.xml. Names invented. */
struct unk_Region {
    const char*     name;           // 0x0: "USA", "EUR", "JAP"
    int             id;             // 0x4
};

/* One string of a string table: its identifier, then (once the table is loaded) its text. */
struct unk_StringEntry {
    u32             id;             // 0x0
    union {
        int             offset;     // 0x4: in the file, in characters from the first text
        const wchar_t*  text;       // 0x4: once loaded
    };
};

/* The start of a string table file: the entries follow it. */
struct unk_StringHeader {
    u32             field_0x0;      // unknown
    u32             count;          // 0x4: number of entries
};

/*
 * A table of the game's texts, read from a "Strings" file: a header, the entries (identifier and
 * offset) and the 16-bit texts. It is handed the file by the loader when it has been read, or its
 * contents directly. Name invented. Size 0x18.
 */
class unk_StringTable : public unk_FileLoadCallback {
public:
                    unk_StringTable( const char* fileName ) :
                        name( fn_801B67FC( fileName, 0 ) ) {}

    /* Takes the file's contents once it has been read. */
    virtual void    fn_800CC364( void* data, int a, const char* fileName, int b, u32 size );

    /* Byte-swaps and indexes the table held in data. */
    void            fn_801D1CFC( void* data );

    char*               name;       // 0x8: the file's name
    unk_StringHeader*   header;     // 0xC: the table, or NULL
    unk_StringEntry*    entries;    // 0x10
    wchar_t*            texts;      // 0x14: after the entries
};

extern unk_Language lbl_80429C68[LANGUAGE_COUNT];
extern unk_Region   lbl_80429CB0[REGION_COUNT];
extern int          lbl_8052A610;   // the language shown, an index in lbl_80429C68
extern int          lbl_8052A614;   // the region, an index in lbl_80429CB0
extern const char*  lbl_805287A8;   // the product identifier of Config.xml
extern const char*  lbl_805287AC;   // the configuration file's name
extern const char*  lbl_805287B0;
extern const char*  lbl_805287BC;
extern const char*  lbl_805287C8;
extern const char*  lbl_805287CC;

/* The string tables loaded so far, newest first. */
extern unk_List< unk_StringTable* > lbl_804908B8;

extern "C" {
// defined in 0x801B3F58 (Climax file layer), unit not yet reconstructed: sizes of the file called
// name; its result is nonzero when *c holds the size of the file's data
int     fn_801B3F58( const char* name, u32* a, u32* b, u32* c );
// defined in 0x801B365C (Climax file layer), unit not yet reconstructed: the size of the file
// called name
u32     fn_801B365C( const char* name );
// defined in 0x801B37B0 (Climax file layer), unit not yet reconstructed: fills dest with the size
// bytes of the file read into data
void    fn_801B37B0( void* data, void* dest, u32 size, int a );
// defined in 0x801B39C0 (Climax font loader), unit not yet reconstructed: loads the font set
// called name
void*   fn_801B39C0( const char* name, int a, int b );
// defined in 0x801AE5CC, unit not yet reconstructed
void    fn_801AE5CC( void* font, int a );
// defined in 0x801CE6B8, unit not yet reconstructed: stores whether the font has double-byte
// characters
void    fn_801CE6B8( u8 japanese );
// defined in 0x801CCC58 (Climax XML), unit not yet reconstructed: the child of node called name
unk_XmlNode* fn_801CCC58( unk_XmlNode* node, const char* name );

void            fn_801D197C();
void            fn_801D1FF0( int index );
int             fn_801D2014();
unk_Language*   fn_801D2040();
void            fn_801D22E0();
const wchar_t*  fn_801D2554( const char* name, int a );
const wchar_t*  fn_801D2630( u32 id, int a );
void            fn_801D26E0();
}

char*   fn_801D207C( const char* name, char* buffer, const char* ext, int a );

#endif
