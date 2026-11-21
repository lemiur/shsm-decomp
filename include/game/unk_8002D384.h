#ifndef GAME_UNK_8002D384_H
#define GAME_UNK_8002D384_H

#include <revolution/types.h>
#include <game/CGenericSpawner.h>
#include <game/ResourceLoader.h>
#include <game/unk_RespotManager.h>

// the sentinel 8 means no entry; the table holds the names that the entries were cached under
extern int           lbl_80526A24;      // .sdata, initial value 8, defined in src/game/unk_8002D384.cpp
extern int           lbl_80526A20;      // .sdata, shared with fn_8002E364 (unit not yet reconstructed)
extern int           lbl_80526A1C;      // .sdata, shared with fn_8002E364 (unit not yet reconstructed)
extern void*         lbl_80529268;      // .sbss, shared with fn_8002E364 (unit not yet reconstructed)
extern const char*   lbl_803E1A90[];    // .data name table, defined in the unit holding 0x803E1A90

extern "C" {
void fn_8002D384( void* handle, void* unusedArg, const char* name );
}

#endif
