// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md. The unit holds the engine's
// localisation: the languages and regions, the Config.xml reader that chooses them, the loaders
// of the "BootStrings" and "Strings" tables and the lookup of a text by its identifier.

#include <climax/Localisation.h>
#include <climax/Memory.h>
#include <extras.h>
#include <stdio.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>

/* The languages in the order of the Locale entry's search; English first, as the fallback. */
unk_Language lbl_80429C68[LANGUAGE_COUNT] = {
    { "Eng", "FontEUR", 0, 1 },
    { "Fre", "FontEUR", 0, 1 },
    { "Ita", "FontEUR", 0, 1 },
    { "Ger", "FontEUR", 0, 1 },
    { "Spa", "FontEUR", 0, 1 },
    { "Jap", "FontJAP", 1, 1 }
};

unk_Region lbl_80429CB0[REGION_COUNT] = {
    { "USA", 0 },
    { "EUR", 1 },
    { "JAP", 2 }
};

int lbl_8052A610;
int lbl_8052A614;

unk_List< unk_StringTable* > lbl_804908B8;

/*
====================
GetCurrentLanguage

The language shown, falling back to the first one when the one chosen is not offered. Name
invented.
====================
*/
static inline int GetCurrentLanguageIndex() {
    if ( !lbl_80429C68[lbl_8052A610].enabled ) {
        lbl_8052A610 = 0;
    }
    return lbl_8052A610;
}

static inline unk_Language& GetCurrentLanguage() {
    return lbl_80429C68[GetCurrentLanguageIndex()];
}

/*
====================
fn_801D197C

Reads Config.xml, then loads the "BootStrings" file of the language shown, and its font set. The
file is "BootStrings.<language>"; in the US region a name whose last character is replaced with
'U' is tried first.
====================
*/
// 0x801D197C
extern "C" void fn_801D197C() {
    char            path[256];
    const char*     file;
    const char*     strings = "BootStrings";
    unk_Language*   language;

    fn_801D26E0();

    language = &GetCurrentLanguage();
    file = NULL;
    if ( lbl_8052A614 == 0 ) {
        sprintf( path, "%s.%s", strings, language->name );
        path[strlen( path ) - 1] = 'U';
        if ( fn_801B3538( path, 0, 0, 1 ) ) {
            file = path;
        }
    }
    if ( file == NULL ) {
        sprintf( path, "%s.%s", strings, language->name );
        if ( fn_801B3538( path, 0, 0, 1 ) ) {
            file = path;
        }
    }

    if ( file != NULL ) {
        unk_StringTable* table = NULL;
        unk_ListNode< unk_StringTable* >* node;

        for ( node = lbl_804908B8.head; node != NULL && table == NULL; node = node->next ) {
            if ( stricmp( node->data->name, file ) == 0 ) {
                table = node->data;
            }
        }
        if ( table == NULL ) {
            table = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_StringTable( file );
            language = &GetCurrentLanguage();
            table->fn_801D1CFC( fn_801B3C40( file, NULL, 0 ) );
            fn_801CE6B8( language->japanese );
            lbl_804908B8.Push( table );
        }
    }

    language = &GetCurrentLanguage();
    fn_801AE5CC( fn_801B39C0( language->font, 0, 0 ), 1 );
}

/*
====================
unk_StringTable::fn_800CC364

Takes the file read by the loader: unpacks data into a new block (RwMalloc'd, kept for the life
of the table) and indexes it.
====================
*/
// 0x801D1C38
void unk_StringTable::fn_800CC364( void* data, int a, const char* fileName, int b, u32 size ) {
    u32     info0 = 0;
    u32     info1 = 0;
    u32     fileSize = 0;
    u32     unpackedSize;
    void*   unpacked;

    if ( fn_801B3F58( fileName, &info0, &info1, &fileSize ) ) {
        unpackedSize = fileSize;
    } else {
        unpackedSize = fn_801B365C( fileName );
    }
    unpacked = RwMalloc( unpackedSize, 0 );
    fn_801B37B0( data, unpacked, unpackedSize, 1 );
    fn_801D1CFC( unpacked );
}

/*
====================
unk_StringTable::fn_801D1CFC

Byte-swaps the header, the entries and the texts of the table held in data and turns each
entry's offset into the address of its text. A text shared by several entries is swapped once.
Format: RenderEclipse-Tools FileFormats.md, String Table; the loader never tests the magic.
====================
*/
// 0x801D1CFC
void unk_StringTable::fn_801D1CFC( void* data ) {
    unk_List< int >     swapped;
    int                 i;

    header = (unk_StringHeader*)data;
    RwMemNative32( &header->field_0x0, 4 );
    RwMemNative32( &header->count, 4 );
    entries = (unk_StringEntry*)( header + 1 );
    texts = (wchar_t*)( entries + header->count );

    for ( i = 0; i < (int)header->count; i++ ) {
        unk_StringEntry* entry = &entries[i];
        int offset;
        unk_ListNode< int >* node;
        bool known = false;

        RwMemNative32( &entry->id, 4 );
        RwMemNative32( &entry->offset, 4 );
        offset = entry->offset;
        entry->text = texts + offset;

        for ( node = swapped.head; node != NULL; node = node->next ) {
            if ( offset == node->data ) {
                known = true;
                break;
            }
        }
        if ( !known ) {
            u16* chars;

            swapped.Push( offset );
            for ( chars = (u16*)entries[i].text; *chars != 0; chars++ ) {
                RwMemNative16( chars, 2 );
            }
        }
    }
}

/*
====================
fn_801D1FF0

Shows language index, if it is offered.
====================
*/
// 0x801D1FF0
extern "C" void fn_801D1FF0( int index ) {
    if ( lbl_80429C68[index].enabled ) {
        lbl_8052A610 = index;
    }
}

/*
====================
fn_801D2014

Falls back to the first language when the one chosen is not offered.
====================
*/
// 0x801D2014
extern "C" int fn_801D2014() {
    return GetCurrentLanguageIndex();
}

/*
====================
fn_801D2040

The language shown.
====================
*/
// 0x801D2040
extern "C" unk_Language* fn_801D2040() {
    return &GetCurrentLanguage();
}

/*
====================
fn_801D207C

Finds the localised version of the file name: its base name (the part before the first '.',
backslashes turned into slashes), the region and language, and the extension ext (or what follows
the dot in name). Looks for <name><region><language>.<ext> first, then <name><language>.<ext>,
then <name>.<ext>, and returns buffer holding the first one that exists, or NULL. The language
is cut to a characters when a is not zero.
====================
*/
// 0x801D207C
char* fn_801D207C( const char* name, char* buffer, const char* ext, int a ) {
    char            language[8];
    char            suffix[256];
    char*           base;
    char*           out;
    char*           file;
    size_t          size;
    unk_Language*   current;

    current = &GetCurrentLanguage();
    size = strlen( name );
    base = new( GAME_HEAP, GAME_HEAP_FLAGS ) char[size + 1];

    for ( out = base; *name != '\0'; ) {
        char c = *name++;

        if ( c == '\\' ) {
            c = '/';
        }
        if ( c == '.' ) {
            if ( ext == NULL ) {
                ext = name;
            }
            break;
        }
        *out++ = c;
    }
    *out = '\0';

    if ( a == 0 ) {
        a = strlen( current->name );
    }
    strncpy( language, current->name, a );
    language[a] = '\0';
    sprintf( suffix, "%s%s", lbl_80429CB0[lbl_8052A614].name, language );

    file = NULL;
    strcpy( buffer, base );
    strcat( buffer, suffix );
    if ( ext != NULL ) {
        strcat( buffer, "." );
        strcat( buffer, ext );
    }
    strupr( buffer );
    if ( fn_801B3538( buffer, 0, 1, 1 ) ) {
        file = buffer;
    }

    if ( file == NULL ) {
        file = NULL;
        strcpy( buffer, base );
        strcat( buffer, language );
        if ( ext != NULL ) {
            strcat( buffer, "." );
            strcat( buffer, ext );
        }
        strupr( buffer );
        if ( fn_801B3538( buffer, 0, 1, 1 ) ) {
            file = buffer;
        }
    }

    if ( file == NULL ) {
        file = NULL;
        strcpy( buffer, base );
        if ( ext != NULL ) {
            strcat( buffer, "." );
            strcat( buffer, ext );
        }
        strupr( buffer );
        if ( fn_801B3538( buffer, 0, 1, 1 ) ) {
            file = buffer;
        }
    }

    delete base;
    return file;
}

/*
====================
fn_801D22E0

Asks the loader for the "Strings" file of the language shown. The file is "Strings.<language>";
in the US region a name whose last character is replaced with 'U' is tried first. The table is
indexed when the file has been read.
====================
*/
// 0x801D22E0
extern "C" void fn_801D22E0() {
    char            path[256];
    const char*     file;
    const char*     strings = "Strings";
    unk_Language*   language;

    language = &GetCurrentLanguage();
    file = NULL;
    if ( lbl_8052A614 == 0 ) {
        sprintf( path, "%s.%s", strings, language->name );
        path[strlen( path ) - 1] = 'U';
        if ( fn_801B3538( path, 0, 0, 1 ) ) {
            file = path;
        }
    }
    if ( file == NULL ) {
        sprintf( path, "%s.%s", strings, language->name );
        if ( fn_801B3538( path, 0, 0, 1 ) ) {
            file = path;
        }
    }

    if ( file != NULL ) {
        unk_StringTable* table = NULL;
        unk_ListNode< unk_StringTable* >* node;

        for ( node = lbl_804908B8.head; node != NULL && table == NULL; node = node->next ) {
            if ( stricmp( node->data->name, file ) == 0 ) {
                table = node->data;
            }
        }
        if ( table == NULL ) {
            table = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_StringTable( file );
            language = &GetCurrentLanguage();
            table->fn_801B250C( file, table, 0.5f );
            fn_801CE6B8( language->japanese );
            lbl_804908B8.Push( table );
        }
    }
}

/*
====================
FindString

The text of the string with the given identifier in the newest table that has one, or NULL. Name
invented.
====================
*/
static inline const wchar_t* FindString( u32 id ) {
    const wchar_t*  text = NULL;

    if ( id != 0 ) {
        unk_ListNode< unk_StringTable* >* node;

        for ( node = lbl_804908B8.head; node != NULL && text == NULL; node = node->next ) {
            unk_StringTable* table = node->data;

            if ( table->header == NULL ) {
                text = NULL;
            } else {
                int i;

                text = NULL;
                for ( i = 0; i < (int)table->header->count; i++ ) {
                    if ( id == table->entries[i].id ) {
                        text = table->entries[i].text;
                        break;
                    }
                }
            }
        }
    }
    return text;
}

/*
====================
fn_801D2554

The text of the string called name: its identifier is a hash of the name with the case bit
cleared (each character masked with 0xDF). When there is none the text is "NO STRING", unless a is
set, which gives NULL.
Format: RenderEclipse-Tools FileFormats.md, String Table; the hash is the code's own.
====================
*/
// 0x801D2554
extern "C" const wchar_t* fn_801D2554( const char* name, int a ) {
    u32             hash = 5381;
    const wchar_t*  text;

    while ( *name != '\0' ) {
        char c = *name++;

        c &= 0xDF;
        hash = ( hash << 5 ) + hash + c;
    }

    text = FindString( hash );
    if ( text == NULL && a == 0 ) {
        text = L"NO STRING";
    }
    return text;
}

/*
====================
fn_801D2630

The text of the string with identifier id; "NO STRING" when there is none, unless a is set, which
gives NULL.
Format: RenderEclipse-Tools FileFormats.md, String Table; the scan is the code's own.
====================
*/
// 0x801D2630
extern "C" const wchar_t* fn_801D2630( u32 id, int a ) {
    const wchar_t*  text = FindString( id );

    if ( text == NULL && a == 0 ) {
        text = L"NO STRING";
    }
    return text;
}

const char* lbl_805287A8 = "NOPRODID";
const char* lbl_805287AC = "Config.xml";
const char* lbl_805287B0 = "ProductID";
const char* lbl_805287BC = "Locale";
const char* lbl_805287C8 = "Region";
const char* lbl_805287CC = "SaveTitle";

/*
====================
fn_801D26E0

Reads Config.xml when it exists: the product identifier, the languages offered (Locale: a list of
codes), the region (Region) and the save title (SaveTitle, read and dropped).
====================
*/
// 0x801D26E0
extern "C" void fn_801D26E0() {
    if ( fn_801B3538( lbl_805287AC, 0, 0, 1 ) ) {
        unk_XmlNode* root = NULL;

        if ( fn_801B6D50( &root, lbl_805287AC ) ) {
            unk_XmlNode* config = fn_801CCC58( root, "CONFIG" );

            if ( config != NULL ) {
                unk_XmlNode* node;
                int i;

                node = fn_801CCB98( config, lbl_805287B0 );
                if ( node != NULL ) {
                    lbl_805287A8 = fn_801B67FC( node->text, 0 );
                }

                node = fn_801CCB98( config, lbl_805287BC );
                if ( node != NULL ) {
                    const char* locales = node->text;

                    for ( i = 0; i < LANGUAGE_COUNT; i++ ) {
                        lbl_80429C68[i].enabled = strstr( locales, lbl_80429C68[i].name ) != NULL;
                    }
                }

                node = fn_801CCB98( config, lbl_805287C8 );
                if ( node != NULL ) {
                    const char* region = node->text;

                    for ( i = 0; i < REGION_COUNT; i++ ) {
                        if ( stricmp( region, lbl_80429CB0[i].name ) == 0 ) {
                            lbl_8052A614 = lbl_80429CB0[i].id;
                        }
                    }
                }

                node = fn_801CCB98( config, lbl_805287CC );
            }
        }
        if ( root != NULL ) {
            RwFree( root );
        }
    }
}

// Compiler-generated, no source of their own: the static initialiser of lbl_804908B8
// (fn_801D285C, 0x801D285C) and the destructor of its class (fn_801D2894, 0x801D2894).
