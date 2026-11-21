#ifndef GAME_UNK_800A26D8_H
#define GAME_UNK_800A26D8_H

#include <revolution/types.h>

struct unk_AttributePacket;

// defined in 0x800A26D8, unit name unconfirmed: the payload of the packet's first system command
// with id 0, NULL if none
const char* fn_800A26D8( const unk_AttributePacket& packet );

#endif
