// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.
/*
 * rtdict.c: the dictionary toolkit. A schema describes a kind of dictionary (its stream chunk IDs
 * and the callbacks that reference, name, stream and destroy its entries) and keeps the
 * dictionaries made with it; a dictionary holds entries found by name. RtDictSchemaCreateDict,
 * RtDictSchemaAddDict, RtDictSchemaRemoveDict, RtDictAddEntry, _rtDictSchemaInitDict and
 * _rtDictDestruct have no copy in retail: each is inlined into its callers. The other functions
 * the header declares (RtDictSchemaForAllDictionaries, RtDictForAllEntries, RtDictRemoveEntry,
 * RtDictStreamWrite, RtDictStreamGetSize) have no copy in retail either (dead-stripped) and are
 * declared only.
 */
#include <rwsdk/rpcriter.h>

/* The toolkit's error ID, which rpdbgerr.h's RWERROR stores in each error it raises. */
#define rwPLUGIN_ID rwID_DICTTOOLKIT

#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/balist.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/rpdbgerr.h>
#include <rwsdk/tool/dict/rtdict.h>

/* Sets up schema with an empty list of dictionaries and no current one (NULL on failure). */
RtDictSchema* RtDictSchemaInit(RtDictSchema* schema) {
    schema->dictionaries = _rwSListCreate(sizeof(RtDict*), rwMEMHINTDUR_GLOBAL | schema->dictChunkType);
    if (schema->dictionaries == NULL) {
        return NULL;
    }

    schema->current = NULL;

    return schema;
} // 0x802F7B3C

/* Releases schema's list of dictionaries; the dictionaries themselves must be destroyed first. */
RwBool RtDictSchemaDestruct(RtDictSchema* schema) {
    _rwSListDestroy(schema->dictionaries);

    return TRUE;
} // 0x802F7B94

/* Adds dict to the dictionaries of schema (NULL on failure). */
RtDictSchema* RtDictSchemaAddDict(RtDictSchema* schema, RtDict* dict) {
    RtDict** slot;

    slot = _rwSListGetNewEntry(schema->dictionaries, rwMEMHINTDUR_EVENT | schema->dictChunkType);
    if (slot == NULL) {
        return NULL;
    }

    *slot = dict;

    return schema;
} // no copy in retail (dead-stripped or inlined)

/* Removes dict from the dictionaries of schema; the last dictionary of the list takes its place. */
RtDictSchema* RtDictSchemaRemoveDict(RtDictSchema* schema, RtDict* dict) {
    RtDict** cur;
    RtDict** end;

    cur = _rwSListGetBegin(schema->dictionaries);
    end = _rwSListGetEnd(schema->dictionaries);
    for (; cur != end; cur++) {
        if (dict == *cur) {
            *cur = *(RtDict**)_rwSListGetEntry(schema->dictionaries,
                                               _rwSListGetNumEntries(schema->dictionaries) - 1);
            _rwSListDestroyEndEntries(schema->dictionaries, 1);
            break;
        }
    }

    return schema;
} // no copy in retail (dead-stripped or inlined)

/* Returns the current dictionary of schema. */
RtDict* RtDictSchemaGetCurrentDict(RtDictSchema* schema) {
    return schema->current;
} // 0x802F7BBC

/* Makes dict the current dictionary of schema. */
RtDictSchema* RtDictSchemaSetCurrentDict(RtDictSchema* schema, RtDict* dict) {
    schema->current = dict;

    return schema;
} // 0x802F7BC4

/*
 * Sets up dictionary as an empty dictionary of schema and adds it to the schema's dictionaries
 * (NULL on failure).
 */
RtDict* _rtDictSchemaInitDict(RtDictSchema* schema, RtDict* dictionary) {
    dictionary->entries = _rwSListCreate(sizeof(RtDictEntryType), rwMEMHINTDUR_EVENT | schema->dictChunkType);
    if (dictionary->entries == NULL) {
        return NULL;
    }

    dictionary->schema = schema;

    if (RtDictSchemaAddDict(schema, dictionary) == NULL) {
        _rwSListDestroy(dictionary->entries);
        return NULL;
    }

    return dictionary;
} // no copy in retail (dead-stripped or inlined)

/* Creates an empty dictionary of schema (NULL on failure). */
RtDict* RtDictSchemaCreateDict(RtDictSchema* schema) {
    RtDict* dictionary;

    dictionary = RwMalloc(sizeof(RtDict), rwMEMHINTDUR_EVENT | schema->dictChunkType);
    if (dictionary == NULL) {
        RWERROR((E_RW_NOMEM, sizeof(RtDict)));
        return NULL;
    }

    if (_rtDictSchemaInitDict(schema, dictionary) == NULL) {
        RwFree(dictionary);
        return NULL;
    }

    return dictionary;
} // no copy in retail (dead-stripped or inlined)

/* Adds entry to dictionary, which takes a reference to it. Returns the entry. */
RtDictEntryType RtDictAddEntry(RtDict* dictionary, RtDictEntryType entry) {
    RtDictEntryType* slot;

    slot = _rwSListGetNewEntry(dictionary->entries, rwMEMHINTDUR_EVENT | dictionary->schema->dictChunkType);
    *slot = entry;
    dictionary->schema->addRefCB(entry);

    return entry;
} // no copy in retail (dead-stripped or inlined)

/* Returns the entry of dictionary named name (NULL if there is none). */
RtDictEntryType RtDictFindNamedEntry(RtDict* dictionary, const RwChar* name) {
    RtDictEntryType* cur;
    RtDictEntryType* end;
    RtDictEntryType entry;
    RtDictEntryGetNameCallBack* getNameCB;

    getNameCB = dictionary->schema->getNameCB;
    end = _rwSListGetEnd(dictionary->entries);
    cur = _rwSListGetBegin(dictionary->entries);
    for (; cur != end; cur++) {
        entry = *cur;

        if (rwstrcmp(getNameCB(entry), name) == 0) {
            return entry;
        }
    }

    return NULL;
} // 0x802F7BCC

/* Releases the dictionary's reference to each of its entries and its list of entries. */
RwBool _rtDictDestruct(RtDict* dictionary) {
    RtDictEntryType* cur;
    RtDictEntryType* end;

    end = _rwSListGetEnd(dictionary->entries);
    cur = _rwSListGetBegin(dictionary->entries);
    while (cur != end) {
        dictionary->schema->destroyCB(*cur++);
    }

    _rwSListDestroy(dictionary->entries);

    return TRUE;
} // no copy in retail (dead-stripped or inlined)

/*
 * Removes dictionary from its schema (clearing the schema's current dictionary if it is this one),
 * releases its entries and frees it.
 */
RwBool RtDictDestroy(RtDict* dictionary) {
    if (dictionary == dictionary->schema->current) {
        dictionary->schema->current = NULL;
    }

    RtDictSchemaRemoveDict(dictionary->schema, dictionary);
    _rtDictDestruct(dictionary);
    RwFree(dictionary);

    return TRUE;
} // 0x802F7C64

/*
 * Reads a dictionary of schema from stream, after its chunk header: a structure chunk holding the
 * number of entries, then one chunk per entry. A stream older than the schema's compatibility
 * version is read by its compatibility callback. Returns the new dictionary (NULL on failure).
 */
RtDict* RtDictSchemaStreamReadDict(RtDictSchema* schema, RwStream* stream) {
    RwUInt32 size;
    RwUInt32 version;
    RwInt32 numEntries;
    RtDict* dictionary;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)) {
        return NULL;
    }

    if (version < schema->compatibilityVersion) {
        return schema->streamReadCompatibilityCB(stream);
    }

    if (!RwStreamRead(stream, &numEntries, size)) {
        return NULL;
    }
    RwMemNative32(&numEntries, sizeof(numEntries));

    dictionary = RtDictSchemaCreateDict(schema);
    if (dictionary == NULL) {
        return NULL;
    }

    while (numEntries--) {
        RtDictEntryType entry;

        if (!RwStreamFindChunk(stream, schema->entryChunkType, &size, &version) ||
            (entry = schema->streamReadCB(stream)) == NULL) {
            RtDictDestroy(dictionary);
            return NULL;
        }

        /* The dictionary takes its own reference; release the one the read gave. */
        RtDictAddEntry(dictionary, entry);
        schema->destroyCB(entry);
    }

    return dictionary;
} // 0x802F7D74
