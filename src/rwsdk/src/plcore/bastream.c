// Reference: tgsm/pbg src/rwsdk/src/plcore/bastream.c
/*
 * bastream.c: RenderWare streams. A stream reads or writes a file, a memory block or a custom
 * device; RwStreamOpen takes its stream from the stream module's free list.
 */
#include <stdio.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/bafsys.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>

/* The free list of streams; RwFreeListCreateAndPreallocateSpace builds it in place here. */
static RwFreeList _rwStreamFreeList;

/* The stream module's registration (offset of its globals in the engine instance). */
static RwModuleInfo streamModule;

/* Streams per free-list block, and blocks allocated when the module opens. */
static RwInt32 _rwStreamFreeListBlockSize = 16;
static RwInt32 _rwStreamFreeListPreallocBlocks = 1;

/* The stream module's globals (RWSTREAMGLOBAL invented). */
typedef struct rwStreamGlobals {
    RwFreeList* streamFreeList; /* 0x0 */
} rwStreamGlobals;

#define RWSTREAMGLOBAL(var) \
    (RWPLUGINOFFSET(rwStreamGlobals, RwEngineInstance, streamModule.globalsOffset)->var)

/* A memory stream grows its block in steps of this many bytes. Name invented. */
#define rwSTREAMMEMORYBLOCKSIZE 512

/* Opens the stream module: creates the stream free list. */
void* _rwStreamModuleOpen(void* instance, RwInt32 offset, RwInt32 size) {
    streamModule.globalsOffset = offset;

    RWSTREAMGLOBAL(streamFreeList) = RwFreeListCreateAndPreallocateSpace(
        sizeof(RwStream), _rwStreamFreeListBlockSize, sizeof(RwUInt32), _rwStreamFreeListPreallocBlocks,
        &_rwStreamFreeList, rwMEMHINTDUR_GLOBAL | rwID_STREAMMODULE);
    if (RWSTREAMGLOBAL(streamFreeList) == NULL) {
        return NULL;
    }

    streamModule.numInstances++;

    return instance;
} // 0x802A48B4

/* Closes the stream module: destroys the stream free list. */
void* _rwStreamModuleClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RWSTREAMGLOBAL(streamFreeList) != NULL) {
        RwFreeListDestroy(RWSTREAMGLOBAL(streamFreeList));
    }

    streamModule.numInstances--;

    return instance;
} // 0x802A4940

/* Opens the file fileName for a file-name stream. */
static RwStream* StreamFileNameInitialize(RwStream* stream, RwStreamAccessType accessType,
                                          const RwChar* fileName) {
    RwStream* result = NULL;
    void* file = NULL;

    switch (accessType) {
    case rwSTREAMREAD:
        file = RwFopen(fileName, "rb");
        break;
    case rwSTREAMWRITE:
        file = RwFopen(fileName, "wb");
        break;
    case rwSTREAMAPPEND:
        file = RwFopen(fileName, "ab");
        break;
    default:
        RWERROR((E_RW_INVSTREAMACCESSTYPE));
        break;
    }

    if (file != NULL) {
        stream->Type.file.fpFile = file;
        result = stream;
    } else {
        RWERROR((E_RW_BADOPEN, fileName));
    }

    return result;
} // 0x802A4994

/* Sets up a memory stream on memory: reading and appending start at its start and end. */
static RwStream* StreamMemoryInitialize(RwStream* stream, RwStreamAccessType accessType,
                                        const RwMemory* memory) {
    RwStream* result = NULL;

    switch (accessType) {
    case rwSTREAMREAD:
        stream->Type.memory.position = 0;
        stream->Type.memory.nSize = memory->length;
        stream->Type.memory.memBlock = memory->start;
        result = stream;
        break;
    case rwSTREAMWRITE:
        stream->Type.memory.position = 0;
        stream->Type.memory.nSize = 0;
        stream->Type.memory.memBlock = NULL;
        result = stream;
        break;
    case rwSTREAMAPPEND:
        stream->Type.memory.position = memory->length;
        stream->Type.memory.nSize = memory->length;
        stream->Type.memory.memBlock = memory->start;
        result = stream;
        break;
    default:
        RWERROR((E_RW_INVSTREAMACCESSTYPE));
        break;
    }

    return result;
} // 0x802A4A98

/*
 * Sets up a file stream on the open file file, which must be able to report its position. Name
 * invented, after pbg's StreamFileNameInitialize and StreamMemoryInitialize.
 */
static RwStream* StreamFileInitialize(RwStream* stream, const void* file) {
    if (RwFtell((void*)file) == -1) {
        return NULL;
    }

    stream->Type.file.constfpFile = file;

    return stream;
} // no copy in retail (inlined)

/* Sets up stream as a stream of type on data (a file, a file name, an RwMemory or an RwStreamCustom). */
RwStream* _rwStreamInitialize(RwStream* stream, RwBool rwOwned, RwStreamType type,
                              RwStreamAccessType accessType, const void* data) {
    RwStream* result = NULL;

    if (stream == NULL) {
        return NULL;
    }

    stream->type = type;
    stream->accessType = accessType;
    stream->rwOwned = rwOwned;

    switch (type) {
    case rwSTREAMFILE:
        result = StreamFileInitialize(stream, data);
        break;
    case rwSTREAMFILENAME:
        result = StreamFileNameInitialize(stream, accessType, (const RwChar*)data);
        break;
    case rwSTREAMMEMORY:
        result = StreamMemoryInitialize(stream, accessType, (const RwMemory*)data);
        break;
    case rwSTREAMCUSTOM:
        memcpy(&stream->Type.custom, data, sizeof(RwStreamCustom));
        result = stream;
        break;
    default:
        RWERROR((E_RW_INVSTREAMTYPE));
        break;
    }

    return result;
} // 0x802A4B54

/* Reads up to length bytes into buffer and returns how many were read. */
RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) {
    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME: {
        void* file = stream->Type.file.fpFile;
        RwUInt32 bytesRead = RwFread(buffer, 1, length, file);

        if (bytesRead != length) {
            if (RwFeof(file)) {
                RWERROR((E_RW_ENDOFSTREAM));
            } else {
                RWERROR((E_RW_READ));
            }
        }

        return bytesRead;
    }
    case rwSTREAMMEMORY:
        if (length > stream->Type.memory.nSize - stream->Type.memory.position) {
            length = stream->Type.memory.nSize - stream->Type.memory.position;
            RWERROR((E_RW_ENDOFSTREAM));
        }

        memcpy(buffer, stream->Type.memory.memBlock + stream->Type.memory.position, length);
        stream->Type.memory.position += length;

        return length;
    case rwSTREAMCUSTOM:
        return stream->Type.custom.sfnread(stream->Type.custom.data, buffer, length);
    default:
        RWERROR((E_RW_INVSTREAMTYPE));
        return 0;
    }
} // 0x802A4C5C

/* Writes length bytes from buffer; a memory stream grows its block as needed. */
RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length) {
    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME: {
        void* file = stream->Type.file.fpFile;
        RwUInt32 bytesWritten = RwFwrite(buffer, 1, length, file);

        if (bytesWritten != length) {
            RWERROR((E_RW_WRITE));
            return NULL;
        }

        return stream;
    }
    case rwSTREAMMEMORY:
        if (stream->Type.memory.memBlock == NULL) {
            stream->Type.memory.memBlock =
                RwMalloc(rwSTREAMMEMORYBLOCKSIZE, rwMEMHINTDUR_EVENT | rwID_STREAMMODULE);
            if (stream->Type.memory.memBlock == NULL) {
                RWERROR((E_RW_NOMEM, rwSTREAMMEMORYBLOCKSIZE));
                return NULL;
            }
            stream->Type.memory.nSize = rwSTREAMMEMORYBLOCKSIZE;
        }

        if (stream->Type.memory.nSize - stream->Type.memory.position < length) {
            RwUInt8* newBlock;
            RwUInt32 newSize = length + stream->Type.memory.nSize;

            if (length < rwSTREAMMEMORYBLOCKSIZE) {
                newSize = stream->Type.memory.nSize + rwSTREAMMEMORYBLOCKSIZE;
            }

            newBlock = RwRealloc(stream->Type.memory.memBlock, newSize,
                                 rwMEMHINTDUR_EVENT | rwMEMHINTFLAG_RESIZABLE | rwID_STREAMMODULE);
            if (newBlock == NULL) {
                RWERROR((E_RW_NOMEM, newSize - stream->Type.memory.nSize));
                return NULL;
            }

            stream->Type.memory.memBlock = newBlock;
            stream->Type.memory.nSize = newSize;
        }

        memcpy(stream->Type.memory.memBlock + stream->Type.memory.position, buffer, length);
        stream->Type.memory.position += length;

        return stream;
    case rwSTREAMCUSTOM:
        if (stream->Type.custom.sfnwrite(stream->Type.custom.data, buffer, length)) {
            return stream;
        }

        return NULL;
    default:
        RWERROR((E_RW_INVSTREAMTYPE));
        return NULL;
    }
} // 0x802A4DE8

/* Skips offset bytes forward. */
RwStream* RwStreamSkip(RwStream* stream, RwUInt32 offset) {
    if (offset == 0) {
        return stream;
    }

    switch (stream->type) {
    case rwSTREAMFILE:
    case rwSTREAMFILENAME: {
        void* file = stream->Type.file.fpFile;

        if (RwFseek(file, offset, SEEK_CUR) != 0) {
            if (RwFeof(file)) {
                RWERROR((E_RW_ENDOFSTREAM));
            }

            stream = NULL;
        }

        return stream;
    }
    case rwSTREAMMEMORY:
        if (stream->Type.memory.position + offset > stream->Type.memory.nSize) {
            stream->Type.memory.position = stream->Type.memory.nSize;
            RWERROR((E_RW_ENDOFSTREAM));
            return NULL;
        }

        stream->Type.memory.position += offset;

        return stream;
    case rwSTREAMCUSTOM:
        if (stream->Type.custom.sfnskip(stream->Type.custom.data, offset)) {
            return stream;
        }

        return NULL;
    default:
        RWERROR((E_RW_INVSTREAMTYPE));
        return NULL;
    }
} // 0x802A5018

/*
 * Closes stream; a memory stream opened for writing hands its block to data (an RwMemory).
 * Frees the stream if RwStreamOpen allocated it.
 */
RwBool RwStreamClose(RwStream* stream, void* data) {
    RwBool result;

    switch (stream->type) {
    case rwSTREAMFILE:
        result = TRUE;
        break;
    case rwSTREAMFILENAME:
        result = RwFclose(stream->Type.file.fpFile) == 0;
        break;
    case rwSTREAMMEMORY:
        if (stream->accessType != rwSTREAMREAD && data != NULL) {
            ((RwMemory*)data)->start = stream->Type.memory.memBlock;
            ((RwMemory*)data)->length = stream->Type.memory.position;
        }
        result = TRUE;
        break;
    case rwSTREAMCUSTOM:
        if (stream->Type.custom.sfnclose != NULL) {
            stream->Type.custom.sfnclose(stream->Type.custom.data);
        }
        result = TRUE;
        break;
    default:
        RWERROR((E_RW_INVSTREAMTYPE));
        return FALSE;
    }

    if (stream->rwOwned) {
        RwFreeListFree(RWSTREAMGLOBAL(streamFreeList), stream);
    }

    return result;
} // 0x802A5168

/* Opens a stream of type on data (see _rwStreamInitialize). */
RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* data) {
    RwStream* stream;

    stream = RwFreeListAlloc(RWSTREAMGLOBAL(streamFreeList), rwMEMHINTDUR_EVENT | rwID_STREAMMODULE);
    if (_rwStreamInitialize(stream, TRUE, type, accessType, data) == NULL) {
        RwFreeListFree(RWSTREAMGLOBAL(streamFreeList), stream);
        stream = NULL;
    }

    return stream;
} // 0x802A527C
