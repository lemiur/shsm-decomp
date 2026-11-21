#ifndef GAME_UNK_8009C308_H
#define GAME_UNK_8009C308_H

#include <game/unk_DecodeThreadContext.h>

// The video decode thread's block (lbl_8046D4D0) and its started flag (lbl_805297A0) are declared in
// unk_DecodeThreadContext.h; their owner, the unit that creates the thread, is not yet split.

extern "C" {
void fn_8009C308(void);
}

#endif
