#ifndef RWSDK_PLCORE_RWCHUNKGROUP_H
#define RWSDK_PLCORE_RWCHUNKGROUP_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest name of a chunk group, without its terminator. */
#define rwCHUNKGROUPMAXNAME 32

/*
 * RwChunkGroup: a named group of chunks in a stream (the RW 3.7 user guide: "a fixed length
 * string").
 */
typedef struct RwChunkGroup {
    RwChar name[rwCHUNKGROUPMAXNAME + 1];   /* 0x0 */
} RwChunkGroup;

void* _rwChunkGroupOpen(void* instance, RwInt32 offset, RwInt32 size);
void* _rwChunkGroupClose(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
