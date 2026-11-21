#ifndef GAME_UNK_800161DC_H
#define GAME_UNK_800161DC_H

#include <revolution/types.h>

// Name of the save archive, "LoadSave.arc" (defined in src/game/unk_800161DC.cpp).
extern const char* lbl_8052693C;

// Two pointer words in small BSS. Word 0 is stored by the function at fn_80016C5C, which is in
// another unit (its owner is not split yet).
extern void* lbl_805291D8[2];

// climax/core, signature unknown: called with the two-word object, the name, a pointer into the
// same object and 0.5f. Declared here until the owning unit is reconstructed.
extern "C" void fn_801B23A0(void** a, const char* name, void** b, float c);

extern "C" {
void fn_800161DC();
}

#endif
