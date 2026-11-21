#ifndef GAME_UNK_8002D2DC_H
#define GAME_UNK_8002D2DC_H

#include <game/CGenericSpawner.h>
#include <game/unk_RespotManager.h>

// Defined elsewhere, unit not yet reconstructed: 0x8002E364 and 0x8002ECE0 read and write it too.
// Holds the resource that fn_8002D2DC last loaded (fn_801A8554's result). Name invented.
extern void* lbl_80529264;

// The load callback that 0x8002D4BC gives to fn_801B05C0 with the zone manager as owner. The
// callback's first and third arguments are the request handle and the asset name; the second
// is unused. Name invented.
extern "C" void fn_8002D2DC( void* handle, u32 unused, const char* name );

#endif
