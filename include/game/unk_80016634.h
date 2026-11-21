#ifndef GAME_UNK_80016634_H
#define GAME_UNK_80016634_H

#include <revolution/types.h>
#include <revolution/nand.h>

// State word of the unit's dispatch (fn_80016644 reads it; fn_80016634 sets it to 2). Name invented.
extern s32 lbl_805291D0;
// Result stored by fn_80016634 and read back by the dispatch. Name invented.
extern s32 lbl_805291D4;

extern "C" {
// Completion callback of every NAND call the dispatcher fn_80016678 starts; the command block is not read.
void fn_80016634( s32 result, NANDCommandBlock* block );
}

#endif
