#ifndef GAME_AREATRIGGERBASE_H
#define GAME_AREATRIGGERBASE_H

// unit name unconfirmed: the class name is the string the unit's static initialiser (0x800D11B0)
// hashes into lbl_80529C28, the class id of the attribute commands of fn_801A73A8 (0x800D0E5C).

#include <climax/Frame.h>
#include <game/CBaseBehaviour.h>

class unk_800D1934;

/*
 * The abstract base of the area triggers (vtable lbl_803ED298, constructor 0x800D0B0C,
 * destructor 0x800D0C04; unit 0x800D0AB0-0x800D1268 not yet reconstructed). An area is a
 * frame-oriented shape: the attribute packet sets its matrix, and the inverse takes points into
 * the shape's space. On its filter message it asks the shape whether the message's point or
 * segment lies inside and tells its listener (0xB4), which calls the enter and exit handlers.
 * Two more messages enable and disable the trigger. Slots are named after their first
 * implementation (AreaTriggerBoxLogic and AreaTriggerBox). Member names invented.
 */
class AreaTriggerBase : public CBaseBehaviour {
public:
                    AreaTriggerBase( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~AreaTriggerBase();
    virtual bool    fn_800D1608( const unk_Vec3* point ) = 0;
    virtual bool    fn_800D1688( const unk_Vec3* segment ) = 0;
    virtual void    fn_800D1818() = 0;
    virtual void    fn_800D1434( void* data ) = 0;     // enter
    virtual void    fn_800D14F0( void* data ) = 0;     // exit

    unk_RwMatrix    matrix;         // 0x34, the area's frame, from the attribute packet
    unk_RwMatrix    inverse;        // 0x74, its inverse: world to area space
    unk_800D1934*   listener;       // 0xB4, owned; decides when to call the enter and exit handlers
    unk_801A92A8    enableMsg;      // 0xB8
    unk_801A92A8    disableMsg;     // 0xC0
    bool            enabled;        // 0xC8
    unk_801A92A8*   filterMsg;      // 0xCC, one of the player, enemy and boss trigger messages
};

// The trigger messages, registered by AreaTriggerBase's static initialiser (0x800D0AB0) under
// these names; their payload is an unk_ColdMsgData (game/unk_800C8598.h).
extern unk_801A92A8 lbl_80529C08;   // "player_trigger"
extern unk_801A92A8 lbl_80529C10;   // "enemy_trigger"
extern unk_801A92A8 lbl_80529C18;   // "boss_trigger"
extern unk_801A92A8 lbl_80529C20;   // "race_start_line_trigger"

#endif
