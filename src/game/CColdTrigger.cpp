// CColdTrigger: class name from the registration strings lbl_803EBF00 and lbl_803EBF10.
// The inline factory fn_800C8988 and the two thunks follow the static initialiser in .text.

#include <game/CColdTrigger.h>

u32 lbl_80529AC8;
u32 lbl_80529ACC = fn_801A7A84( "CColdTrigger" );
unk_801A8F6C lbl_80529AD0( "CColdTrigger", CColdTrigger::fn_800C8988, sizeof( CColdTrigger ) );

/*
 * v limited to [lo, hi]. Name invented. Every argument is taken by const reference: literal limits
 * become .sdata temporaries (lbl_80527510 and lbl_80527514 here, read through their addresses).
 */
template< class T >
inline const T& Clamp( const T& v, const T& lo, const T& hi ) {
    if ( v > hi ) {
        return hi;
    }
    if ( v < lo ) {
        return lo;
    }
    return v;
}

/*
====================
CColdTrigger::~CColdTrigger
====================
*/
// 0x800C875C
CColdTrigger::~CColdTrigger() {
    fn_801A9E00( coldMsg );
    fn_801A9940( coldMsg );
}

/*
====================
CColdTrigger::fn_801A73A8

Takes the name of the message and the cold level from the attribute packet.
====================
*/
// 0x800C87F0
void CColdTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529ACC );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetColdMsg:
            fn_801AA218( coldMsg, it->GetString(), 0 );
            break;
        case CMD_SetColdLevel:
            coldLevel = it->GetU32();
            // the limits are int: the unsigned level is clamped through an int temporary
            coldLevel = Clamp< int >( coldLevel, 0, 3 );
            break;
        }
        ++it;
    }
}

/*
====================
CColdTrigger::fn_8009CB1C

Sets the cold level when the message arrives.
====================
*/
// 0x800C88D8
void CColdTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == coldMsg.record ) {
        lbl_80529AC8 = coldLevel;
    }
}
