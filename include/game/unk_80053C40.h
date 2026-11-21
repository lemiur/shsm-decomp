#ifndef GAME_UNK_80053C40_H
#define GAME_UNK_80053C40_H
// Placeholder class unk_80053C40 (invented name): the object is 0xA64 bytes (the size passed to new in fn_800550E4)
// and derives from unk_80055174 at offset 0 (the constructor calls fn_80055174 first). Its CEventHandler
// subobject sits at 0x24 inside the base chain. fn_80054464 is a zone-change override that calls the base
// (unk_80055174::fn_801A7BFC) and then registers the two zone-change ids. Not yet declared as a class.

#include <revolution/types.h>

extern "C" {
void fn_80053C40();
void fn_80053E68();
void fn_80053F70();
void fn_80054188();
int fn_800542A4();
void fn_800542AC();
void fn_80054464();
void fn_80054508();
void fn_800545C8();
void fn_80054728();
void fn_80054F0C();
void fn_80055068();
void fn_80055070();
void fn_8005507C();
void fn_800550E4();
void fn_8005512C();
void fn_80055134();
void fn_8005513C();
void fn_80055144();
void fn_8005514C();
void fn_80055154();
void fn_8005515C();
void fn_80055164();
void fn_8005516C();
}

#endif
