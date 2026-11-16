// unit name unconfirmed: RenderWare's ba* file naming, no string or reference names this file.
/*
 * batkbin.c: plugin data in streams. An object's plugin data is written as one extension chunk
 * holding a chunk per plugin; on reading, chunks of plugins not registered are skipped.
 * No reference source: reconstructed from the retail code.
 */
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/batkreg.h>

/* Sets the stream callbacks of the plugin pluginID; returns its data offset, or -1 if it is not registered. */
RwInt32 _rwPluginRegistryAddPluginStream(RwPluginRegistry* reg, RwUInt32 pluginID,
                                         RwPluginDataChunkReadCallBack readCB,
                                         RwPluginDataChunkWriteCallBack writeCB,
                                         RwPluginDataChunkGetSizeCallBack getSizeCB) {
    RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->pluginID == pluginID) {
            break;
        }
        entry = entry->nextRegEntry;
    }

    if (entry != NULL) {
        entry->readCB = readCB;
        entry->writeCB = writeCB;
        entry->getSizeCB = getSizeCB;

        return entry->offset;
    }

    return -1;
} // 0x802A5328

/* Sets the callback called after every read of an object, whether or not the plugin's chunk was present. */
RwInt32 _rwPluginRegistryAddPlgnStrmlwysCB(RwPluginRegistry* reg, RwUInt32 pluginID,
                                           RwPluginDataChunkAlwaysCallBack alwaysCB) {
    RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->pluginID == pluginID) {
            break;
        }
        entry = entry->nextRegEntry;
    }

    if (entry != NULL) {
        entry->alwaysCB = alwaysCB;

        return entry->offset;
    }

    return -1;
} // 0x802A536C

/* Sets the callback that _rwPluginRegistryInvokeRights calls. */
RwInt32 _rwPluginRegistryAddPlgnStrmRightsCB(RwPluginRegistry* reg, RwUInt32 pluginID,
                                             RwPluginDataChunkRightsCallBack rightsCB) {
    RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->pluginID == pluginID) {
            break;
        }
        entry = entry->nextRegEntry;
    }

    if (entry != NULL) {
        entry->rightsCB = rightsCB;

        return entry->offset;
    }

    return -1;
} // 0x802A53A8

/* Reads the extension chunk of object: each plugin reads its own chunk, then every always callback runs. */
const RwPluginRegistry* _rwPluginRegistryReadDataChunks(const RwPluginRegistry* reg, RwStream* stream,
                                                        void* object) {
    RwUInt32 size;
    RwUInt32 version;
    RwPluginRegEntry* entry;

    if (!RwStreamFindChunk(stream, rwID_EXTENSION, &size, &version)) {
        return NULL;
    }

    if (version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION) {
        while (size != 0) {
            RwUInt32 chunkType;
            RwUInt32 chunkLength;

            if (!_rwStreamReadChunkHeader(stream, &chunkType, &chunkLength, NULL, NULL)) {
                return NULL;
            }

            entry = reg->firstRegEntry;
            while (entry != NULL) {
                if (entry->pluginID == chunkType) {
                    break;
                }
                entry = entry->nextRegEntry;
            }

            if (entry != NULL && entry->readCB != NULL) {
                if (!entry->readCB(stream, chunkLength, object, entry->offset, entry->size)) {
                    return NULL;
                }
            } else if (!RwStreamSkip(stream, chunkLength)) {
                return NULL;
            }

            size -= chunkLength + rwCHUNKHEADERSIZE;
        }

        for (entry = reg->firstRegEntry; entry != NULL; entry = entry->nextRegEntry) {
            if (entry->alwaysCB != NULL) {
                if (!entry->alwaysCB(object, entry->offset, entry->size)) {
                    return NULL;
                }
            }
        }

        return reg;
    }

    RWERROR((E_RW_BADVERSION));
    return NULL;
} // 0x802A53E4

/* Calls the rights callback of the plugin id on obj; NULL if it has none or the callback fails. */
const RwPluginRegistry* _rwPluginRegistryInvokeRights(const RwPluginRegistry* reg, RwUInt32 id,
                                                      void* obj, RwUInt32 extraData) {
    RwPluginRegEntry* entry = reg->firstRegEntry;

    while (entry != NULL) {
        if (entry->pluginID == id) {
            break;
        }
        entry = entry->nextRegEntry;
    }

    if (entry != NULL && entry->rightsCB != NULL) {
        if (entry->rightsCB(obj, entry->offset, entry->size, extraData)) {
            return reg;
        }

        return NULL;
    }

    return NULL;
} // 0x802A559C

/* The size of the plugin chunks of object, with their chunk headers (not the extension chunk's). */
RwInt32 _rwPluginRegistryGetSize(const RwPluginRegistry* reg, const void* object) {
    RwInt32 size = 0;
    RwPluginRegEntry* entry;

    for (entry = reg->firstRegEntry; entry != NULL; entry = entry->nextRegEntry) {
        if (entry->getSizeCB != NULL) {
            RwInt32 pluginSize = entry->getSizeCB(object, entry->offset, entry->size);

            if (pluginSize > 0) {
                size += pluginSize + rwCHUNKHEADERSIZE;
            }
        }
    }

    return size;
} // 0x802A5628

/* Writes the extension chunk of object, a chunk for each plugin that has data to write. */
const RwPluginRegistry* _rwPluginRegistryWriteDataChunks(const RwPluginRegistry* reg, RwStream* stream,
                                                         const void* object) {
    RwPluginRegEntry* entry;

    if (!RwStreamWriteChunkHeader(stream, rwID_EXTENSION, _rwPluginRegistryGetSize(reg, object))) {
        return NULL;
    }

    for (entry = reg->firstRegEntry; entry != NULL; entry = entry->nextRegEntry) {
        if (entry->getSizeCB != NULL && entry->writeCB != NULL) {
            RwInt32 size = entry->getSizeCB(object, entry->offset, entry->size);

            if (size > 0) {
                if (!RwStreamWriteChunkHeader(stream, entry->pluginID, size)) {
                    return NULL;
                }

                if (!entry->writeCB(stream, size, object, entry->offset, entry->size)) {
                    return NULL;
                }
            }
        }
    }

    return reg;
} // 0x802A56AC

/* Skips the extension chunk of an object. */
const RwPluginRegistry* _rwPluginRegistrySkipDataChunks(const RwPluginRegistry* reg, RwStream* stream) {
    RwUInt32 size;

    if (!RwStreamFindChunk(stream, rwID_EXTENSION, &size, NULL)) {
        return NULL;
    }

    while (size != 0) {
        RwUInt32 chunkLength;

        if (!_rwStreamReadChunkHeader(stream, NULL, &chunkLength, NULL, NULL)) {
            return NULL;
        }

        if (!RwStreamSkip(stream, chunkLength)) {
            return NULL;
        }

        size -= chunkLength + rwCHUNKHEADERSIZE;
    }

    return reg;
} // 0x802A57F8
