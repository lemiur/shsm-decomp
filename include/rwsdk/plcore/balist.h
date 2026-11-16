// Reference: tgsm/pbg include/rwsdk/plcore/balist.h
#ifndef RWSDK_PLCORE_BALIST_H
#define RWSDK_PLCORE_BALIST_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RwSList: a growable array of fixed-size entries. */
typedef struct RwSList {
    RwUInt8* listElements;          /* 0x0 */
    RwInt32 numElementsFilled;      /* 0x4 */
    RwInt32 numElementsAlloced;     /* 0x8 */
    RwInt32 entrySize;              /* 0xC */
} RwSList;

RwSList* _rwSListCreate(RwInt32 size, RwUInt32 hint);
void* _rwSListGetNewEntry(RwSList* sList, RwUInt32 hint);
void* _rwSListGetNewEntries(RwSList* sList, RwInt32 n, RwUInt32 hint);
void _rwSListDestroyEndEntries(RwSList* sList, RwInt32 amount);
RwBool _rwSListDestroy(RwSList* sList);
RwInt32 _rwSListGetNumEntries(const RwSList* sList);
void* _rwSListGetEntry(RwSList* sList, RwInt32 entry);
void* _rwSListGetBegin(RwSList* sList);
void* _rwSListGetEnd(RwSList* sList);

#ifdef __cplusplus
}
#endif

#endif
