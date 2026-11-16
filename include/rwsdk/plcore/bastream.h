// Reference: tgsm/pbg include/rwsdk/plcore/bastream.h
#ifndef RWSDK_PLCORE_BASTREAM_H
#define RWSDK_PLCORE_BASTREAM_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwStreamType: what a stream reads from or writes to. */
typedef enum RwStreamType {
    rwNASTREAM = 0,
    rwSTREAMFILE = 1,       /* an open file */
    rwSTREAMFILENAME = 2,   /* a file the stream opens and closes */
    rwSTREAMMEMORY = 3,     /* a memory block */
    rwSTREAMCUSTOM = 4,     /* the caller's callbacks */
    rwSTREAMTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwStreamType;

/* RwStreamAccessType: how a stream is opened. */
typedef enum RwStreamAccessType {
    rwNASTREAMACCESS = 0,
    rwSTREAMREAD = 1,
    rwSTREAMWRITE = 2,
    rwSTREAMAPPEND = 3,
    rwSTREAMACCESSTYPEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwStreamAccessType;

/* RwStreamMemory: a memory stream's block and position. */
typedef struct RwStreamMemory {
    RwUInt32 position;  /* 0x0 */
    RwUInt32 nSize;     /* 0x4: size of memBlock */
    RwUInt8* memBlock;  /* 0x8 */
} RwStreamMemory;

/* RwStreamFile: a file stream's file. */
typedef union RwStreamFile {
    void* fpFile;
    const void* constfpFile;
} RwStreamFile;

typedef RwBool (*rwCustomStreamFnClose)(void* data);
typedef RwUInt32 (*rwCustomStreamFnRead)(void* data, void* buffer, RwUInt32 length);
typedef RwBool (*rwCustomStreamFnWrite)(void* data, const void* buffer, RwUInt32 length);
typedef RwBool (*rwCustomStreamFnSkip)(void* data, RwUInt32 offset);

/* RwStreamCustom: a custom stream's callbacks and their data. */
typedef struct RwStreamCustom {
    rwCustomStreamFnClose sfnclose; /* 0x0 */
    rwCustomStreamFnRead sfnread;   /* 0x4 */
    rwCustomStreamFnWrite sfnwrite; /* 0x8 */
    rwCustomStreamFnSkip sfnskip;   /* 0xC */
    void* data;                     /* 0x10 */
} RwStreamCustom;

typedef union RwStreamUnion {
    RwStreamMemory memory;
    RwStreamFile file;
    RwStreamCustom custom;
} RwStreamUnion;

/* RwStream: a binary stream. */
typedef struct RwStream {
    RwStreamType type;              /* 0x0 */
    RwStreamAccessType accessType;  /* 0x4 */
    RwInt32 position;               /* 0x8 */
    RwStreamUnion Type;             /* 0xC */
    RwBool rwOwned;                 /* 0x20: allocated by RwStreamOpen */
} RwStream;

/* RwMemory: a memory block, as passed to a memory stream. */
typedef struct RwMemory {
    RwUInt8* start;     /* 0x0 */
    RwUInt32 length;    /* 0x4 */
} RwMemory;

void* _rwStreamModuleOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwStreamModuleClose(void* instance, RwInt32 offset, RwInt32 size);
RwStream* _rwStreamInitialize(RwStream* stream, RwBool rwOwned, RwStreamType type,
                              RwStreamAccessType accessType, const void* data);
RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* data);
RwBool RwStreamClose(RwStream* stream, void* data);
RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length);
RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length);
RwStream* RwStreamSkip(RwStream* stream, RwUInt32 offset);

#ifdef __cplusplus
}
#endif

#endif
