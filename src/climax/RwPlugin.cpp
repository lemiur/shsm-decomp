// Unit name and boundary: Gormiti link map (RwPlugin.obj).
// Names and layout: Gormiti link map and DWARF (ParticleX RwPlugin.obj).
// ClimaxP1AtomicPrimeOffset, ClimaxP1AtomicPluginAttach, ClimaxP1DictionaryPrimeOffset and
// ClimaxP1DictionaryPluginAttach have no copy in retail (inlined).
/*
 * RwPlugin.cpp: the RenderWare plugins of the particle effects. One is registered with the atomics
 * and holds an atomic's index in its effect entry; the other is registered with the engine and keeps
 * the schema of the dictionary of effect entries (ClimaxP1Entry: the particle objects of an effect).
 */

#include <climax/RwPlugin.h>
#include <climax/ClimaxS1Lib.h>
#include <climax/ParticleX.h>
#include <climax/RwInterface.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/balist.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/tool/dict/rtdict.h>
#include <rwsdk/world/baclump.h>
#include <string.h>

// The plugin IDs of the atomic plugin and the dictionary, and the stream chunk IDs of the dictionary and
// its entries.
#define CLIMAXP1_ATOMIC_PLUGIN_ID 0xA07
#define CLIMAXP1_DICTIONARY_PLUGIN_ID 0xA0A
#define CLIMAXP1_DICT_CHUNK_ID 0xA0A
#define CLIMAXP1_ENTRY_CHUNK_ID 0xA09

// The dictionaries a schema preallocates room for.
#define CLIMAXP1_DICTIONARIES 60

#define CLIMAXP1_ATOMIC_DATA( atomic ) RWPLUGINOFFSET( ClimaxP1AtomicData, atomic, lbl_80528858 )
#define CLIMAXP1_ATOMIC_CONST_DATA( atomic ) RWPLUGINOFFSETCONST( ClimaxP1AtomicData, atomic, lbl_80528858 )

// The render callback of the first atomic read; invented name.
static RpAtomicCallBackRender RenderCallBack;

static void* ClimaxP1AtomicDataConstructor( void* atomic, int offset, int size );
static void* ClimaxP1AtomicDataDestructor( void* atomic, int offset, int size );
static void* ClimaxP1AtomicDataCopier( void* dstAtomic, const void* srcAtomic, int offset, int size );
static RwStream* ClimaxP1AtomicDataReadStream( RwStream* stream, int binaryLength, void* atomic, int offset, int size );
static RwStream* ClimaxP1AtomicDataWriteStream( RwStream* stream, int binaryLength, const void* atomic, int offset, int size );
static int ClimaxP1AtomicDataGetStreamSize( const void* atomic, int offset, int size );
static void* ClimaxP1EntryAddRef( void* entry );
static int ClimaxP1EntryDestroyRef( void* entry );
static const char* ClimaxP1EntryGetName( const void* entry );
static unsigned int ClimaxP1EntryStreamGetSize( const void* entry );
static void* ClimaxP1EntryStreamRead( RwStream* stream );
static void* ClimaxP1EntryStreamWrite( const void* entry, RwStream* stream );
static void* ClimaxP1DictionaryGlobalDataConstructor( void* clump, int offset, int size );
static void* ClimaxP1DictionaryGlobalDataDestructor( void* clump, int offset, int size );

/*
====================
ClimaxP1AtomicDataConstructor

A new atomic has no index and no chain item.
====================
*/
// 0x801F35FC
static void* ClimaxP1AtomicDataConstructor( void* atomic, int offset, int size ) {
    CLIMAXP1_ATOMIC_DATA( atomic )->index = -1;
    CLIMAXP1_ATOMIC_DATA( atomic )->item = NULL;
    CLIMAXP1_ATOMIC_DATA( atomic )->unk_8 = 0;
    return atomic;
}

/*
====================
ClimaxP1AtomicDataDestructor

Destroys the atomic's chain item. The test reads the item through the read-only plugin macro (Gormiti's
Debug build computes the plugin address once for the test and again for the call): retail reloads the item
after the test, which a read through the non-const macro or a local data pointer does not give.
====================
*/
// 0x801F3628
static void* ClimaxP1AtomicDataDestructor( void* atomic, int offset, int size ) {
    if ( CLIMAXP1_ATOMIC_CONST_DATA( atomic )->item != NULL ) {
        fn_801EDABC( CLIMAXP1_ATOMIC_DATA( atomic )->item );
    }
    return atomic;
}

/*
====================
ClimaxP1AtomicDataCopier

A copy has the index of its source and no chain item of its own.
====================
*/
// 0x801F3670
static void* ClimaxP1AtomicDataCopier( void* dstAtomic, const void* srcAtomic, int offset, int size ) {
    CLIMAXP1_ATOMIC_DATA( dstAtomic )->index = CLIMAXP1_ATOMIC_DATA( srcAtomic )->index;
    CLIMAXP1_ATOMIC_DATA( dstAtomic )->item = NULL;
    return dstAtomic;
}

/*
====================
ClimaxP1AtomicDataReadStream

Reads the atomic's index. The first atomic read also keeps its render callback.
====================
*/
// 0x801F3690
static RwStream* ClimaxP1AtomicDataReadStream( RwStream* stream, int binaryLength, void* atomic, int offset, int size ) {
    if ( !RwStreamReadInt32( stream, &CLIMAXP1_ATOMIC_DATA( atomic )->index, sizeof( int ) ) ) {
        return NULL;
    }
    if ( RenderCallBack == NULL ) {
        RenderCallBack = ( (RpAtomic*)atomic )->renderCallBack;
    }
    return stream;
}

/*
====================
ClimaxP1AtomicDataWriteStream

Writes the atomic's index.
====================
*/
// 0x801F36FC
static RwStream* ClimaxP1AtomicDataWriteStream( RwStream* stream, int binaryLength, const void* atomic, int offset, int size ) {
    if ( !RwStreamWriteInt32( stream, &CLIMAXP1_ATOMIC_DATA( atomic )->index, sizeof( int ) ) ) {
        return NULL;
    }
    return stream;
}

/*
====================
ClimaxP1AtomicDataGetStreamSize
====================
*/
// 0x801F3748
static int ClimaxP1AtomicDataGetStreamSize( const void* atomic, int offset, int size ) {
    return sizeof( int );
}

/*
====================
ClimaxP1EntryAddRef

Takes a reference to the entry.
====================
*/
// 0x801F3750
static void* ClimaxP1EntryAddRef( void* entry ) {
    ( (ClimaxP1Entry*)entry )->refCount++;
    return entry;
}

/*
====================
ClimaxP1EntryDestroyRef

Releases a reference to the entry; the last reference destroys it.
====================
*/
// 0x801F3760
static int ClimaxP1EntryDestroyRef( void* entry ) {
    ClimaxP1Entry* p1Entry = (ClimaxP1Entry*)entry;

    if ( p1Entry->refCount > 1 ) {
        p1Entry->refCount--;
        return TRUE;
    }
    fn_801EDA30( p1Entry );
    return TRUE;
}

/*
====================
ClimaxP1EntryGetName
====================
*/
// 0x801F37A0
static const char* ClimaxP1EntryGetName( const void* entry ) {
    return "ClimaxP1Entry";
}

/*
====================
ClimaxP1EntryStreamGetSize

The size of an entry's chunk data: the object count and the attribute blocks of its objects.
====================
*/
// 0x801F37AC
static unsigned int ClimaxP1EntryStreamGetSize( const void* entry ) {
    unsigned int size;
    int i;
    int imax;
    const ClimaxP1Entry* p1Entry;
    ParticleXObject** ObjPtr;

    p1Entry = (const ClimaxP1Entry*)entry;
    size = sizeof( int );
    ObjPtr = p1Entry->ObjectPtrs;
    for ( i = 0, imax = p1Entry->NumObjects; i < imax; i++, ObjPtr++ ) {
        ParticleXObject* Obj = *ObjPtr;
        if ( Obj != NULL ) {
            size += ClimaxS1DataGetSize( Obj->RawData );
            size += ClimaxS1DataGetSize( Obj->ChildRawData );
        }
    }
    return size;
}

/*
====================
ClimaxP1EntryStreamRead

A new entry from its chunk data: the object count, then the attribute blocks of each object. The
objects are made, set up from their blocks and given the chain templates of the emitters; the blocks
are freed after that. A failed read leaves the entry unfreed.
====================
*/
// 0x801F381C
static void* ClimaxP1EntryStreamRead( RwStream* stream ) {
    ClimaxP1Entry* entry;
    int nP1Objects = 0;
    int i;
    int imax;

    if ( !RwStreamReadInt32( stream, &nP1Objects, sizeof( int ) ) ) {
        return NULL;
    }

    entry = ClimaxP1EntryCreate( nP1Objects );
    if ( entry != NULL ) {
        entry->refCount = 1;

        ParticleXObject** ObjPtr = entry->ObjectPtrs;
        imax = nP1Objects;
        for ( i = 0; i < imax; i++, ObjPtr++ ) {
            ParticleXObject* NewObj;
            int NewObjType;
            int NewObjSize = sizeof( ParticleXObject );
            char* RawData = ClimaxS1DataStreamRead( stream );
            if ( RawData == NULL ) {
                return NULL;
            }
            char* ChildRawData = ClimaxS1DataStreamRead( stream );
            if ( ChildRawData == NULL ) {
                return NULL;
            }

            NewObjType = ClimaxS1DataGetValueI( RawData, 1 );
            switch ( NewObjType ) {
            case PARTICLEX_EMITTER_BASIC:
                NewObjSize = sizeof( ParticleX_Emitter_Basic );
                break;
            case PARTICLEX_EMITTER_SPHERE:
                NewObjSize = sizeof( ParticleX_Emitter_Sphere );
                break;
            case PARTICLEX_EMITTER_TORUS:
                NewObjSize = sizeof( ParticleX_Emitter_Torus );
                break;
            case PARTICLEX_EMITTER_VERTEX:
                NewObjSize = sizeof( ParticleX_Emitter_Vertex );
                break;
            case PARTICLEX_EMITTER_CONTAINER:
                NewObjSize = sizeof( ParticleX_Emitter_Container );
                break;
            case PARTICLEX_CONTAINER:
                NewObjSize = sizeof( ParticleX_Container );
                break;
            case PARTICLEX_FIELD:
                NewObjSize = sizeof( ParticleX_Field );
                break;
            case PARTICLEX_FIELD_LOCAL:
                NewObjSize = sizeof( ParticleX_Field );
                break;
            }

            NewObj = (ParticleXObject*)RwOsGetMemoryInterface()->rwmalloc( NewObjSize, 0 );
            memset( NewObj, 0, NewObjSize );
            NewObj->RawData = RawData;
            NewObj->ChildRawData = ChildRawData;
            NewObj->Type = NewObjType;
            *ObjPtr = NewObj;
        }

        ObjPtr = entry->ObjectPtrs;
        for ( i = 0; i < imax; i++, ObjPtr++ ) {
            ParticleXObject* Obj = *ObjPtr;
            switch ( Obj->Type ) {
            case PARTICLEX_EMITTER_BASIC:
                ( (ParticleX_Emitter_Basic*)Obj )->Setup();
                break;
            case PARTICLEX_EMITTER_SPHERE:
                ( (ParticleX_Emitter_Sphere*)Obj )->Setup();
                break;
            case PARTICLEX_EMITTER_TORUS:
                ( (ParticleX_Emitter_Torus*)Obj )->Setup();
                break;
            case PARTICLEX_EMITTER_VERTEX:
                ( (ParticleX_Emitter_Vertex*)Obj )->Setup();
                break;
            case PARTICLEX_EMITTER_CONTAINER:
                ( (ParticleX_Emitter_Container*)Obj )->Setup();
                break;
            case PARTICLEX_CONTAINER:
                ( (ParticleX_Container*)Obj )->Setup();
                break;
            case PARTICLEX_FIELD:
                ( (ParticleX_Field*)Obj )->Setup();
                break;
            case PARTICLEX_FIELD_LOCAL:
                ( (ParticleX_Field*)Obj )->Setup();
                break;
            }
        }

        ObjPtr = entry->ObjectPtrs;
        for ( i = 0; i < imax; i++, ObjPtr++ ) {
            ParticleX_Emitter* Obj = (ParticleX_Emitter*)*ObjPtr;
            if ( Obj->Type >= PARTICLEX_EMITTER_BASIC && Obj->Type <= PARTICLEX_EMITTER_VERTEX ) {
                Obj->ChainTemplate = ClimaxP1CreateChainTemplate( i, entry );
            }
        }

        ObjPtr = entry->ObjectPtrs;
        for ( i = 0; i < imax; i++, ObjPtr++ ) {
            ParticleXObject* Obj = *ObjPtr;
            ClimaxS1DataDestroy( Obj->RawData );
            Obj->RawData = NULL;
            ClimaxS1DataDestroy( Obj->ChildRawData );
            Obj->ChildRawData = NULL;
        }
    }
    return entry;
}

/*
====================
ClimaxP1EntryStreamWrite

Entries are never written; the original's body is empty.
====================
*/
// 0x801F3A8C
static void* ClimaxP1EntryStreamWrite( const void* entry, RwStream* stream ) {
}

// The schema of the dictionary: its stream chunks, and the callbacks that manage its entries.
static RtDictSchema ClimaxP1Schema = {
    "ClimaxP1DictionarySchema",
    CLIMAXP1_DICT_CHUNK_ID,
    CLIMAXP1_ENTRY_CHUNK_ID,
    rwLIBRARYBASEVERSION,
    NULL,
    NULL,
    (RtDictEntryAddRefCallBack*)ClimaxP1EntryAddRef,
    (RtDictEntryDestroyCallBack*)ClimaxP1EntryDestroyRef,
    (RtDictEntryGetNameCallBack*)ClimaxP1EntryGetName,
    (RtDictEntryStreamGetSizeCallBack*)ClimaxP1EntryStreamGetSize,
    (RtDictEntryStreamReadCallBack*)ClimaxP1EntryStreamRead,
    NULL,
    (RtDictEntryStreamWriteCallBack*)ClimaxP1EntryStreamWrite,
};

/*
====================
ClimaxP1DictionaryGlobalDataConstructor

Initialises the schema when an engine instance opens, keeps it in the plugin's data and makes
room for the dictionaries.
====================
*/
// 0x801F3A90
static void* ClimaxP1DictionaryGlobalDataConstructor( void* clump, int offset, int size ) {
    *RWPLUGINOFFSET( RtDictSchema*, RwEngineInstance, GlobalOffset ) = RtDictSchemaInit( &ClimaxP1Schema );
    _rwSListGetNewEntries( ClimaxP1Schema.dictionaries, CLIMAXP1_DICTIONARIES, ClimaxP1Schema.dictChunkType );
    ClimaxP1Schema.dictionaries->numElementsFilled = 0;
    return clump;
}

/*
====================
ClimaxP1DictionaryGlobalDataDestructor
====================
*/
// 0x801F3AFC
static void* ClimaxP1DictionaryGlobalDataDestructor( void* clump, int offset, int size ) {
    RtDictSchemaDestruct( &ClimaxP1Schema );
    return clump;
}

/*
====================
ClimaxP1DictionaryGetSchema
====================
*/
// 0x801F3B34
RtDictSchema* ClimaxP1DictionaryGetSchema() {
    return *RWPLUGINOFFSET( RtDictSchema*, RwEngineInstance, GlobalOffset );
}

// no copy in retail (inlined)
static int ClimaxP1AtomicPrimeOffset() {
    lbl_80528858 = RpAtomicGetPluginOffset( CLIMAXP1_ATOMIC_PLUGIN_ID );
    return lbl_80528858;
}

// no copy in retail (inlined)
static int ClimaxP1AtomicPluginAttach() {
    if ( ClimaxP1AtomicPrimeOffset() >= 0 ) {
        return TRUE;
    }
    lbl_80528858 = RpAtomicRegisterPlugin( sizeof( ClimaxP1AtomicData ), CLIMAXP1_ATOMIC_PLUGIN_ID,
                                           ClimaxP1AtomicDataConstructor, ClimaxP1AtomicDataDestructor,
                                           ClimaxP1AtomicDataCopier );
    if ( lbl_80528858 < 0 ) {
        return FALSE;
    }

    int offset = -1;
    offset = RpAtomicRegisterPluginStream( CLIMAXP1_ATOMIC_PLUGIN_ID, ClimaxP1AtomicDataReadStream,
                                           ClimaxP1AtomicDataWriteStream, ClimaxP1AtomicDataGetStreamSize );
    if ( offset != lbl_80528858 ) {
        return FALSE;
    }
    return TRUE;
}

// no copy in retail (inlined)
static int ClimaxP1DictionaryPrimeOffset() {
    GlobalOffset = RwEngineGetPluginOffset( CLIMAXP1_DICTIONARY_PLUGIN_ID );
    return GlobalOffset >= 0;
}

// no copy in retail (inlined)
static int ClimaxP1DictionaryPluginAttach() {
    if ( ClimaxP1DictionaryPrimeOffset() ) {
        return TRUE;
    }
    GlobalOffset = RwEngineRegisterPlugin( sizeof( RtDictSchema* ), CLIMAXP1_DICTIONARY_PLUGIN_ID,
                                           ClimaxP1DictionaryGlobalDataConstructor,
                                           ClimaxP1DictionaryGlobalDataDestructor );
    return GlobalOffset >= 0;
}

/*
====================
ClimaxP1EffectPluginAttach

Registers the atomic plugin and the dictionary. Returns false when either fails. The inlined atomic
attach follows Gormiti's Debug build: the prime returns the offset, and the stream registration's
result goes through the initialised local offset before the compare.
====================
*/
// 0x801F3B44
int ClimaxP1EffectPluginAttach() {
    if ( !ClimaxP1AtomicPluginAttach() ) {
        return FALSE;
    }
    return ClimaxP1DictionaryPluginAttach() != 0;
}
