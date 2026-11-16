/*
 * FMOD::DSPConnectionPool: the system's pool of DSP connections (fmod_dsp_connectionpool.cpp).
 * Connections come in blocks of 128, each block with its own array of list nodes and one block of
 * level data; alloc adds a block when the free list is empty. Slot 0 of mConnection, mNodeMemory and
 * mLevelData is the base of its block; the *Memory arrays keep the pointers the allocator returned.
 * Layout and member names: Gormiti DWARF (sibling.exe types DSPConnectionPool), size 0xA28. No vtable.
 */
#ifndef _FMOD_DSP_CONNECTIONPOOL_H
#define _FMOD_DSP_CONNECTIONPOOL_H

#include "fmod.h"
#include "fmod_linkedlist.h"

namespace FMOD {

class DSPConnectionI;
class SystemI;

class DSPConnectionPool {
public:
    SystemI        *mSystem;                    // 0x00
    DSPConnectionI *mConnection[128];           // 0x04
    DSPConnectionI *mConnectionMemory[128];     // 0x204
    LinkedListNode *mNodeMemory[128];           // 0x404
    int             mNumInputLevels;            // 0x604
    int             mNumOutputLevels;           // 0x608
    int             mNumConnections;            // 0x60C, connections per block
    LinkedListNode  mUsedListHead;              // 0x610
    LinkedListNode  mFreeListHead;              // 0x61C
    float          *mLevelData[128];            // 0x628
    float          *mLevelDataMemory[128];      // 0x828

    // numconnections is invented (the DWARF records no name); it is rounded up to a multiple of 128.
    FMOD_RESULT init( SystemI *system, int numconnections, int numoutputlevels, int numinputlevels );  // 0x8035D474
    FMOD_RESULT close();                                                       // 0x8035D6F8
    FMOD_RESULT alloc( DSPConnectionI **connection, bool protect );            // 0x8035D7D4
    FMOD_RESULT free( DSPConnectionI *connection, bool protect );              // 0x8035DBFC
};

} // namespace FMOD

#endif /* _FMOD_DSP_CONNECTIONPOOL_H */
