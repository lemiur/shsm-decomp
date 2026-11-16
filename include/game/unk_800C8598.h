#ifndef GAME_UNK_800C8598_H
#define GAME_UNK_800C8598_H

#include <game/CBaseBehaviour.h>
#include <game/Messages.h>
#include <climax/Frame.h>

/* The payload unk_800C8598 forwards. Name and member names invented. */
struct unk_ColdMsgData {
    CEventHandler*  sender;     // 0x00
    CZone*          zone;       // 0x04, the behaviour's zone
    unk_RwV3d       pos;        // 0x08
    u8              field_0x14[0xC];
    int             type;       // 0x20, 2 here
};

/*
 * Forwards lbl_8052A780 to a message given at construction, with the position of a
 * behaviour, while lbl_80527508 is set. An embedded member of the unit at 0x80055464
 * (at 0x150C, forwarding lbl_80529C08). vtable __vt__12unk_800C8598, size 0x14. Name invented.
 */
class unk_800C8598 : public CEventHandler {
public:
                    unk_800C8598( unk_801A92A8* forwardMsg );
    virtual         ~unk_800C8598();
    virtual void    fn_8009CB1C( unk_Msg& msg );

    CBaseBehaviour* behaviour;      // 0x08
    bool            enabled;        // 0x0C
    unk_801A92A8*   forwardMsg;     // 0x10
};

extern bool lbl_80527508;   // true: unk_800C8598 forwards; also written by the unit at 0x800AEFE4

#endif
