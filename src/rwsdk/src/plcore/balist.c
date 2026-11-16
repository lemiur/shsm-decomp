// Reference: tgsm/pbg src/rwsdk/src/plcore/balist.c
/*
 * balist.c: RwSList, a growable array of fixed-size entries. The array starts with room for
 * SLIST_STARTSIZE entries and grows by a quarter (plus what is asked for) when it is full.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/balist.h>

/* Entries a new list has room for. */
#define SLIST_STARTSIZE 20

/* Creates a list of entries of size bytes; its array is allocated with hint. */
RwSList* _rwSListCreate(RwInt32 size, RwUInt32 hint) {
    RwSList* sList = (RwSList*)RwMalloc(sizeof(RwSList), rwMEMHINTDUR_EVENT);

    if (sList == NULL) {
        RWERROR((E_RW_NOMEM, sizeof(RwSList)));
        return NULL;
    }

    sList->numElementsFilled = 0;
    sList->numElementsAlloced = SLIST_STARTSIZE;
    sList->entrySize = size;

    sList->listElements = (RwUInt8*)RwMalloc(sList->numElementsAlloced * sList->entrySize,
                                                hint | rwMEMHINTFLAG_RESIZABLE);
    if (sList->listElements == NULL) {
        RWERROR((E_RW_NOMEM, sList->numElementsAlloced * sList->entrySize));
        RwFree(sList);
        return NULL;
    }

    return sList;
} // 0x802A0FE4

/* Appends an entry to a list, growing its array when it is full; NULL if it cannot grow. */
void* _rwSListGetNewEntry(RwSList* sList, RwUInt32 hint) {
    void* entry;

    if (sList->numElementsFilled >= sList->numElementsAlloced) {
        RwUInt8* newElements = (RwUInt8*)RwRealloc(
            sList->listElements,
            sList->entrySize * (sList->numElementsAlloced + sList->numElementsAlloced / 4), hint);

        if (newElements == NULL) {
            RWERROR((E_RW_NOMEM,
                     sList->entrySize * (sList->numElementsAlloced + sList->numElementsAlloced / 4)));
            return NULL;
        }

        sList->listElements = newElements;
        sList->numElementsAlloced = sList->numElementsAlloced + sList->numElementsAlloced / 4;
    }

    entry = sList->listElements + sList->numElementsFilled * sList->entrySize;
    sList->numElementsFilled++;

    return entry;
} // 0x802A10FC

/* Appends n entries to a list, growing its array when they do not fit; NULL if it cannot grow. */
void* _rwSListGetNewEntries(RwSList* sList, RwInt32 n, RwUInt32 hint) {
    void* entries;

    if (sList->numElementsFilled + n >= sList->numElementsAlloced) {
        RwUInt8* newElements = (RwUInt8*)RwRealloc(
            sList->listElements,
            sList->entrySize * (sList->numElementsAlloced + n + sList->numElementsAlloced / 4), hint);

        if (newElements == NULL) {
            RWERROR((E_RW_NOMEM,
                     sList->entrySize * (sList->numElementsAlloced + n + sList->numElementsAlloced / 4)));
            return NULL;
        }

        sList->listElements = newElements;
        sList->numElementsAlloced = sList->numElementsAlloced + n + sList->numElementsAlloced / 4;
    }

    entries = sList->listElements + sList->numElementsFilled * sList->entrySize;
    sList->numElementsFilled += n;

    return entries;
} // 0x802A11E0

/* Removes the last amount entries of a list. */
void _rwSListDestroyEndEntries(RwSList* sList, RwInt32 amount) {
    sList->numElementsFilled -= amount;
} // 0x802A12DC

/* Destroys a list and its array. */
RwBool _rwSListDestroy(RwSList* sList) {
    if (sList->listElements != NULL && sList->numElementsAlloced != 0) {
        RwFree(sList->listElements);
        sList->listElements = NULL;
        sList->numElementsAlloced = 0;
    }

    RwFree(sList);

    return TRUE;
} // 0x802A12EC

/* The number of entries in a list. */
RwInt32 _rwSListGetNumEntries(const RwSList* sList) {
    return sList->numElementsFilled;
} // 0x802A1364

/* The entry'th entry of a list. */
void* _rwSListGetEntry(RwSList* sList, RwInt32 entry) {
    return sList->listElements + sList->entrySize * entry;
} // 0x802A136C

/* The first entry of a list. */
void* _rwSListGetBegin(RwSList* sList) {
    return sList->listElements;
} // 0x802A1380

/* The end of a list's entries (one past the last). */
void* _rwSListGetEnd(RwSList* sList) {
    return sList->listElements + sList->entrySize * sList->numElementsFilled;
} // 0x802A1388
