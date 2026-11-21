#ifndef GAME_UNK_ANIMCONTROLLER_H
#define GAME_UNK_ANIMCONTROLLER_H

#include <revolution/types.h>
#include <stddef.h>
#include <game/unk_803E0A48.h>

/*
 * Animation controller owned by unk_8001777C at field_0x148 (partial layout). Name invented.
 *
 * Entries sit in a pointer array from begin to end. slotCount is the number of slots in use,
 * liveCount the number of non-NULL ones; a removed slot stays NULL until the array shrinks past
 * it. The array grows one slot at a time.
 */
class unk_AnimController {
public:
    int             fn_8001A100( const char* name, int payload, bool reuseHole );
    bool            fn_8001A428( int index );
    int             fn_8001A520( const char* name, bool );  // the second argument is passed by the caller and not read

    unk_803E0A48** entries;        // 0x0
    unk_803E0A48** entriesEnd;     // 0x4
    unk_803E0A48** entriesCapEnd;  // 0x8
    int             slotCount;      // 0xC
    int             liveCount;      // 0x10
};

#endif
