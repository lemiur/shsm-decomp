// Unit name and boundary: Gormiti link map (ClimaxT1Dictionary.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxT1Dictionary.obj).
// ClimaxT1DictionaryPrimeOffset and ClimaxT1EntryCreate have no copy in retail (dead-stripped).
/*
 * ClimaxT1Dictionary.cpp: the dictionary of the ClimaxT1 effects. An RtDict schema of animations,
 * registered as an engine plugin whose per-instance data is the schema itself; the entries are
 * ClimaxT1Entry objects that hold one RtAnimAnimation each.
 */

#include <climax/ClimaxT1Dictionary.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/balist.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65): the chunk headers it writes carry that number.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

// The plugin ID of the dictionary plugin, and the stream chunk IDs of the dictionary and its entries.
#define CLIMAXT1_PLUGIN_ID 0xA04
#define CLIMAXT1_DICT_CHUNK_ID 0xA04
#define CLIMAXT1_ENTRY_CHUNK_ID 0xA05

// The dictionaries a schema preallocates room for.
#define CLIMAXT1_DICTIONARIES 100

static int GlobalOffset = -1;

static void* ClimaxT1EntryAddRef( void* entry );
static int ClimaxT1EntryDestroy( void* entry );
static const char* ClimaxT1EntryGetName( const void* entry );
static unsigned int ClimaxT1EntryStreamGetSize( const void* entry );
static void* ClimaxT1EntryStreamRead( RwStream* stream );
static void* ClimaxT1EntryStreamWrite( const void* entry, RwStream* stream );
static void* ClimaxT1DictionaryGlobalDataConstructor( void* clump, int offset, int size );
static void* ClimaxT1DictionaryGlobalDataDestructor( void* clump, int offset, int size );

/*
====================
ClimaxT1EntryAddRef

Takes a reference to the entry.
====================
*/
// 0x801DD340
static void* ClimaxT1EntryAddRef( void* entry ) {
    ClimaxT1Entry* climaxEntry = (ClimaxT1Entry*)entry;

    climaxEntry->referenceCount++;
    return entry;
}

/*
====================
ClimaxT1EntryDestroy

Releases a reference to the entry; the last reference frees it with its animation.
====================
*/
// 0x801DD350
static int ClimaxT1EntryDestroy( void* entry ) {
    ClimaxT1Entry* climaxEntry = (ClimaxT1Entry*)entry;

    if ( climaxEntry->referenceCount > 1 ) {
        climaxEntry->referenceCount--;
        return TRUE;
    }
    if ( climaxEntry->animation != NULL ) {
        RtAnimAnimationDestroy( climaxEntry->animation );
    }
    RwFree( climaxEntry );
    return TRUE;
}

/*
====================
ClimaxT1EntryGetName
====================
*/
// 0x801DD3BC
static const char* ClimaxT1EntryGetName( const void* entry ) {
    const ClimaxT1Entry* climaxEntry = (const ClimaxT1Entry*)entry;

    return climaxEntry->name;
}

/*
====================
ClimaxT1EntryStreamGetSize

The size of an entry's chunk data: its animation index and the animation's chunk.
====================
*/
// 0x801DD3C4
static unsigned int ClimaxT1EntryStreamGetSize( const void* entry ) {
    const ClimaxT1Entry* climaxEntry = (const ClimaxT1Entry*)entry;
    unsigned int size;

    size = sizeof( int ) + rwCHUNKHEADERSIZE;
    size += RtAnimAnimationStreamGetSize( climaxEntry->animation );
    return size;
}

/*
====================
ClimaxT1EntryStreamRead

A new entry from its chunk data: the animation index, then the animation. NULL when there is no
animation.
====================
*/
// 0x801DD3EC
static void* ClimaxT1EntryStreamRead( RwStream* stream ) {
    ClimaxT1Entry* entry;

    entry = (ClimaxT1Entry*)RwMalloc( sizeof( ClimaxT1Entry ), 0 );
    if ( entry == NULL ) {
        return NULL;
    }

    entry->name = "ClimaxT1Entry";
    entry->referenceCount = 1;
    entry->animation = NULL;
    RwStreamReadInt32( stream, &entry->animationIndex, sizeof( int ) );
    if ( RwStreamFindChunk( stream, rwID_HANIMANIMATION, NULL, NULL ) ) {
        entry->animation = RtAnimAnimationStreamRead( stream );
    }
    if ( entry->animation == NULL ) {
        RwFree( entry );
        return NULL;
    }
    return entry;
}

/*
====================
ClimaxT1EntryStreamWrite

Writes an entry's chunk: the animation index, then the animation. Returns the entry, or NULL on
failure.
====================
*/
// 0x801DD4C8
static void* ClimaxT1EntryStreamWrite( const void* entry, RwStream* stream ) {
    const ClimaxT1Entry* climaxEntry = (const ClimaxT1Entry*)entry;
    int index;

    if ( !RwStreamWriteChunkHeader( stream, CLIMAXT1_ENTRY_CHUNK_ID, ClimaxT1EntryStreamGetSize( entry ) ) ) {
        return NULL;
    }
    index = climaxEntry->animationIndex;
    if ( !RwStreamWriteInt32( stream, &index, sizeof( int ) ) ||
         !RtAnimAnimationStreamWrite( climaxEntry->animation, stream ) ) {
        return NULL;
    }
    return (void*)entry;
}

// The schema of the dictionary: its stream chunks, and the callbacks that manage its entries.
static RtDictSchema ClimaxT1Schema = {
    "ClimaxT1DictionarySchema",
    CLIMAXT1_DICT_CHUNK_ID,
    CLIMAXT1_ENTRY_CHUNK_ID,
    rwLIBRARYBASEVERSION,
    NULL,
    NULL,
    (RtDictEntryAddRefCallBack*)ClimaxT1EntryAddRef,
    (RtDictEntryDestroyCallBack*)ClimaxT1EntryDestroy,
    (RtDictEntryGetNameCallBack*)ClimaxT1EntryGetName,
    (RtDictEntryStreamGetSizeCallBack*)ClimaxT1EntryStreamGetSize,
    (RtDictEntryStreamReadCallBack*)ClimaxT1EntryStreamRead,
    NULL,
    (RtDictEntryStreamWriteCallBack*)ClimaxT1EntryStreamWrite,
};

/*
====================
ClimaxT1DictionaryGlobalDataConstructor

Initialises the schema when an engine instance opens, keeps it in the plugin's data and makes
room for the dictionaries.
====================
*/
// 0x801DD570
static void* ClimaxT1DictionaryGlobalDataConstructor( void* clump, int offset, int size ) {
    *RWPLUGINOFFSET( RtDictSchema*, RwEngineInstance, GlobalOffset ) = RtDictSchemaInit( &ClimaxT1Schema );
    _rwSListGetNewEntries( ClimaxT1Schema.dictionaries, CLIMAXT1_DICTIONARIES, ClimaxT1Schema.dictChunkType );
    ClimaxT1Schema.dictionaries->numElementsFilled = 0;
    return clump;
}

/*
====================
ClimaxT1DictionaryGlobalDataDestructor
====================
*/
// 0x801DD5DC
static void* ClimaxT1DictionaryGlobalDataDestructor( void* clump, int offset, int size ) {
    RtDictSchemaDestruct( &ClimaxT1Schema );
    return clump;
}

/*
====================
ClimaxT1DictionaryPluginAttach

Registers the dictionary as an engine plugin. Returns false when the engine has no room for it.
====================
*/
// 0x801DD614
extern "C" int ClimaxT1DictionaryPluginAttach() {
    GlobalOffset = RwEngineGetPluginOffset( CLIMAXT1_PLUGIN_ID );
    if ( GlobalOffset >= 0 ) {
        return TRUE;
    }
    GlobalOffset = RwEngineRegisterPlugin( sizeof( RtDictSchema* ), CLIMAXT1_PLUGIN_ID,
                                           ClimaxT1DictionaryGlobalDataConstructor,
                                           ClimaxT1DictionaryGlobalDataDestructor );
    return GlobalOffset >= 0;
}

/*
====================
ClimaxT1DictionaryGetSchema
====================
*/
// 0x801DD674
extern "C" RtDictSchema* ClimaxT1DictionaryGetSchema() {
    return *RWPLUGINOFFSET( RtDictSchema*, RwEngineInstance, GlobalOffset );
}

/*
====================
ClimaxT1DictionarySchemaGetAnimation

The animation with the index, in the schema's current dictionary; NULL when it has none.
====================
*/
// 0x801DD684
extern "C" RtAnimAnimation* ClimaxT1DictionarySchemaGetAnimation( int animation ) {
    RtDict* pDict = ClimaxT1DictionaryGetSchema()->current;

    if ( pDict != NULL ) {
        if ( animation >= 0 && animation < pDict->entries->numElementsFilled ) {
            ClimaxT1Entry** entry = (ClimaxT1Entry**)pDict->entries->listElements;

            return entry[animation]->animation;
        }
    }
    return NULL;
}

/*
====================
ClimaxT1DictionaryGetAnimation

The animation with the index, in the dictionary; NULL when it has none.
====================
*/
// 0x801DD6D0
extern "C" RtAnimAnimation* ClimaxT1DictionaryGetAnimation( RtDict* cmDict, int animation ) {
    RtDict* pDict = cmDict;

    if ( pDict != NULL ) {
        if ( animation >= 0 && animation < pDict->entries->numElementsFilled ) {
            ClimaxT1Entry** entry = (ClimaxT1Entry**)pDict->entries->listElements;

            return entry[animation]->animation;
        }
    }
    return NULL;
}
