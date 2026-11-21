#ifndef GAME_UNK_800AF298_H
#define GAME_UNK_800AF298_H

#include <revolution/types.h>
#include <game/CBaseBehaviour.h>
#include <climax/EventHandler.h>
#include <climax/unk_801A92A8.h>
#include <game/unk_800B0C64.h>

namespace Movie {

/*
 * Movie::CMovieTrigger: the class name comes from the dtk signature of its factory
 * MakeNew__Q25Movie13CMovieTriggerFRCQ23RWS16CAttributePacket in this unit. Its destructor
 * releases the message at 0x34 (the same shape as CColdTrigger's). Member names invented;
 * the rest of the layout is not yet reconstructed.
 */
class CMovieTrigger : public CBaseBehaviour {
public:
    virtual         ~CMovieTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    unk_801A92A8    msg;            // 0x34, name invented
    const char*     field_0x3c;     // 0x3C
    bool            field_0x40;     // 0x40
};

}

extern u32 lbl_80529890;   // .sbss, the attribute-table id the trigger's packet is read with (name unknown)

extern "C" {
void fn_800AF32C();
void fn_800AF498();
void fn_800AF79C();
void fn_800AFA04();
void fn_800AFAB4();
void fn_800AFB40();
void fn_800AFB44();
void fn_800AFDCC();
void fn_800AFE7C();
void fn_800B012C();
void fn_800B01B0();
void fn_800B05FC();
void fn_800B0B80();
void fn_800B0C44();
void fn_800B0C4C();
void fn_800B0C54();
void fn_800B0C5C();
}

#endif
