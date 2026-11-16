#ifndef RWSDK_TOOL_DICT_RTDICT_H
#define RWSDK_TOOL_DICT_RTDICT_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/balist.h>
#include <rwsdk/plcore/bastream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Declares a dictionary schema defined by the toolkit or plugin that owns it. */
#define RTDICTSCHEMADECLARE(schema) extern RtDictSchema schema;

typedef struct RtDictSchema RtDictSchema;
typedef struct RtDict RtDict;

/* An entry of a dictionary: an object the schema's callbacks know how to handle. */
typedef void* RtDictEntryType;
typedef const void* RtDictEntryConstType;

/* Registers that the caller holds a reference to entry; returns the entry. */
typedef void*(RtDictEntryAddRefCallBack)(RtDictEntryType entry);
/* Releases a reference to entry, destroying it with its last reference. */
typedef RwBool(RtDictEntryDestroyCallBack)(RtDictEntryType entry);
/* Returns the name entry is found by. */
typedef const RwChar*(RtDictEntryGetNameCallBack)(RtDictEntryConstType entry);
/* Returns the size of entry when streamed. */
typedef RwUInt32(RtDictEntryStreamGetSizeCallBack)(RtDictEntryConstType entry);
/* Reads a new entry from stream, holding one reference (NULL on failure). */
typedef RtDictEntryType(RtDictEntryStreamReadCallBack)(RwStream* stream);
/* Reads a whole dictionary of an older stream version (NULL on failure). */
typedef RtDict*(RtDictStreamReadCompatibilityCallBack)(RwStream* stream);
/* Writes entry to stream; returns the entry (NULL on failure). */
typedef const RtDictEntryType(RtDictEntryStreamWriteCallBack)(RtDictEntryConstType entry, RwStream* stream);

/* Applied to each dictionary of a schema by RtDictSchemaForAllDictionaries. */
typedef RtDict*(RtDictCallBack)(RtDict* dictionary, void* data);
/* Applied to each entry of a dictionary by RtDictForAllEntries. */
typedef RtDictEntryType(RtDictEntryCallBack)(RtDictEntryType entry, void* data);

/* RtDict: a container of entries found by name, created and managed through its schema. */
struct RtDict {
    RtDictSchema* schema;       /* 0x0: the schema that manages the dictionary */
    RwSList* entries;           /* 0x4: the entries (RtDictEntryType each) */
};

/*
 * RtDictSchema: the layout and management callbacks of a kind of dictionary. It keeps the
 * dictionaries made with it and a current one.
 */
struct RtDictSchema {
    const RwChar* name;                                         /* 0x0: for debugging */
    RwUInt32 dictChunkType;                                     /* 0x4: a dictionary's stream chunk ID */
    RwUInt32 entryChunkType;                                    /* 0x8: an entry's stream chunk ID */
    RwUInt32 compatibilityVersion;                              /* 0xC: older streams use streamReadCompatibilityCB */
    RwSList* dictionaries;                                      /* 0x10: the dictionaries (RtDict* each) */
    RtDict* current;                                            /* 0x14 */
    RtDictEntryAddRefCallBack* addRefCB;                        /* 0x18 */
    RtDictEntryDestroyCallBack* destroyCB;                      /* 0x1C */
    RtDictEntryGetNameCallBack* getNameCB;                      /* 0x20 */
    RtDictEntryStreamGetSizeCallBack* streamGetSizeCB;          /* 0x24 */
    RtDictEntryStreamReadCallBack* streamReadCB;                /* 0x28 */
    RtDictStreamReadCompatibilityCallBack* streamReadCompatibilityCB; /* 0x2C */
    RtDictEntryStreamWriteCallBack* streamWriteCB;              /* 0x30 */
};

RtDictSchema* RtDictSchemaInit(RtDictSchema* schema);
RwBool RtDictSchemaDestruct(RtDictSchema* schema);
RtDict* RtDictSchemaStreamReadDict(RtDictSchema* schema, RwStream* stream);
RtDict* RtDictSchemaCreateDict(RtDictSchema* schema);
RtDictSchema* RtDictSchemaAddDict(RtDictSchema* schema, RtDict* dict);
RtDictSchema* RtDictSchemaRemoveDict(RtDictSchema* schema, RtDict* dict);
RwBool RtDictSchemaForAllDictionaries(RtDictSchema* schema, RtDictCallBack* callback, RtDictEntryType data);
RtDict* RtDictSchemaGetCurrentDict(RtDictSchema* schema);
RtDictSchema* RtDictSchemaSetCurrentDict(RtDictSchema* schema, RtDict* dict);
RwBool RtDictDestroy(RtDict* dictionary);
RtDictEntryType RtDictAddEntry(RtDict* dictionary, RtDictEntryType entry);
RtDictEntryType RtDictFindNamedEntry(RtDict* dictionary, const RwChar* name);
const RtDict* RtDictForAllEntries(const RtDict* dictionary, RtDictEntryCallBack* callBack, RtDictEntryType data);
RtDict* RtDictRemoveEntry(RtDict* dictionary, RtDictEntryType entry);
const RtDict* RtDictStreamWrite(const RtDict* dictionary, RwStream* stream);
RwUInt32 RtDictStreamGetSize(const RtDict* dictionary);

RtDict* _rtDictSchemaInitDict(RtDictSchema* schema, RtDict* dictionary);
RwBool _rtDictDestruct(RtDict* dictionary);

#ifdef __cplusplus
}
#endif

#endif
