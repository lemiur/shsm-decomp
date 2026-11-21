#ifndef GAME_UNK_8009C88C_H
#define GAME_UNK_8009C88C_H

// unit name unconfirmed: no string or class-name evidence names this file.

#include <revolution/types.h>

struct unk_AttributePacket;

// The class id the attribute iterators are built with. The unit that stores it (fn_8009CEEC,
// 0x8009CEEC) is not reconstructed yet; the retail .sbss word is defined there.
extern u32 lbl_805297A8;

bool fn_8009C88C( const char* name, const unk_AttributePacket& packet );

#endif
