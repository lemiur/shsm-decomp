#ifndef GAME_UNK_800531A0_H
#define GAME_UNK_800531A0_H

#include <revolution/types.h>

/*
 * Layout facts recovered from the retail assembly, for the class that this unit's constructor
 * (0x800531A0) and destructor (0x800532CC) build. Class name invented (unk_800531A0, from the
 * constructor address); not yet a class declaration.
 *   - The factory fn_80053BB0 allocates 0x7A0 bytes (operator new(0x7A0, 2, 0x40000)) and calls the
 *     constructor with the factory's own argument, which the constructor never reads.
 *   - The constructor first calls fn_80055174 on the new object, then stores six vptrs: lbl_803E355C+0x8
 *     at 0x0, +0x38 at 0x24 (the CEventHandler sub-object, which it passes to CEventHandler::fn_801A9DE8),
 *     +0x0 at 0x2C, +0x58 at 0x34, +0xA4 at 0x3C, +0x124 at 0x218 and +0x22C at 0x700.
 *   - lbl_803E355C (680 bytes) is one table of several vtable slices, in .data order in the unit's
 *     assembly; the slices hold this unit's functions and the overrides of the bases.
 *   - The constructor registers five messages (lbl_8052A788, lbl_8052A790, lbl_805298E8, lbl_805298F0,
 *     lbl_805298F8) through CEventHandler::fn_801A9DE8 and sets the byte at 0x367 to 1.
 *   - The destructor fn_800532CC unregisters the messages, releases the clump at 0x130 (RpClumpDestroy)
 *     and the asset list at 0x788, destroys the unk_801A92A8 member at 0x798, calls the base destructor
 *     fn_8005524C and deletes the object when its flag argument is positive.
 */

extern "C" {
void fn_800531A0();
void fn_800532CC();
void fn_800533F0();
void fn_800533F4();
void fn_800533F8();
void fn_80053448();
void fn_80053508();
int fn_80053668();
void fn_80053670();
void fn_8005378C();
void fn_80053830();
void fn_80053834();
void fn_800539A4();
void fn_800539C4();
void fn_800539F4();
void fn_800539F8();
void fn_80053A8C();
void fn_80053A90();
void fn_80053A94();
void fn_80053A98();
void fn_80053B38();
void fn_80053B48();
void fn_80053BB0();
void fn_80053BF8();
void fn_80053C00();
void fn_80053C08();
void fn_80053C10();
void fn_80053C18();
void fn_80053C20();
void fn_80053C28();
void fn_80053C30();
void fn_80053C38();
}

#endif
