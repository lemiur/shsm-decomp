#ifndef GAME_UNK_MESSAGEINBOX_H
#define GAME_UNK_MESSAGEINBOX_H

#include <revolution/types.h>
#include <game/unk_HUDController.h>

/*
 * The phone's message inbox, as the ambient text message and voicemail behaviours use it
 * (units not yet reconstructed). Header and type names invented.
 */

/* The five-word message record handed to the inbox by const reference. Name invented. */
struct unk_MessageInfo {
            unk_MessageInfo( char* a, char* b, char* c, char* d, u32 e )
                : field_0x0( a ), field_0x4( b ), field_0x8( c ), field_0xC( d ), field_0x10( e ) {}

    char*   field_0x0;
    char*   field_0x4;
    char*   field_0x8;
    char*   field_0xC;
    u32     field_0x10;
};

/*
 * The nine-word record handed to the inbox when a phone call starts, by const reference. Name
 * invented.
 */
struct unk_CallInfo {
            // quirk: the texts arrive and are stored in the caller's order 0x54, 0x5C, 0x58 (the
            // third text before the second); retail's stores run in this order, and any other
            // order leaves CPhoneCallTrigger's message handler unmatched
            unk_CallInfo( char* a, char* c, char* b, u32 d, u32 e, u32 f, u32 g, char* h, u32 i ) {
                field_0x0 = a;
                field_0x8 = c;
                field_0x4 = b;
                field_0xC = d;
                field_0x10 = e;
                field_0x14 = f;
                field_0x18 = g;
                field_0x1C = h;
                field_0x20 = i;
            }

    char*   field_0x0;
    char*   field_0x4;
    char*   field_0x8;
    u32     field_0xC;
    u32     field_0x10;
    u32     field_0x14;
    u32     field_0x18;
    char*   field_0x1C;
    u32     field_0x20;
};

class CPhoneCallTrigger;
class CBaseBehaviour;

/* The object at 0x9C of *lbl_80529D20 that takes new messages. Name invented. */
class unk_MessageInbox {
public:
    void    fn_800F56F0( const unk_MessageInfo& info );    // text messages; defined elsewhere, unit not yet reconstructed
    void    fn_800F5AE8( const unk_MessageInfo& info );    // voicemails; defined elsewhere, unit not yet reconstructed
    void    fn_800F5EE0( const unk_CallInfo& info, CPhoneCallTrigger* caller );    // phone calls; defined elsewhere, unit not yet reconstructed
    void    fn_800F6158();                                  // defined elsewhere, unit not yet reconstructed
    void    fn_800F67E4();                                  // defined elsewhere, unit not yet reconstructed
    void    fn_800F6B6C( bool flag );                       // defined elsewhere, unit not yet reconstructed
};

/* The type of *lbl_80529D20. Name invented. */
class unk_InboxOwner {
public:
    u8                  field_0x0[0x9C];
    unk_MessageInbox    field_0x9C;     // 0x9C
    u8                  field_0x9D[0xB];
    u32                 field_0xA8;     // 0xA8, a flag CAmbientInterferencePoint reads
    u8                  field_0xAC[0x6C4 - 0xAC];
    unk_HUDController   field_0x6C4;    // 0x6C4
    u8                  field_0x6C5[0x1FEC - 0x6C5];
    u32                 field_0x1FEC;   // 0x1FEC, 1 when the pad device is not a Freestyle pointer (fn_80009F60 sets it)

    // defined in 0x800E97C8, unit not yet reconstructed: drops behaviour from the list at 0xDA8
    // (fn_800D2334 unlinks and frees the node whose key it is); the parameter type is
    // provisional (the callers seen pass a behaviour)
    void                fn_800E97C8( CBaseBehaviour* behaviour );
    // defined in 0x800E97C0, unit not yet reconstructed: files behaviour with a state in the list
    // at 0xDA8 (fn_800D21B8 updates its node, or adds one); CDrawingTest passes 2, 4 and 13;
    // parameter names invented
    void                fn_800E97C0( CBaseBehaviour* behaviour, int state, bool a, bool b );

    // defined in 0x800ED314 and 0x800ED3C8, unit not yet reconstructed: CDrawingTest calls them
    // with (0x18, 0) when painting starts and stops; parameter types provisional (ED314 tests bit
    // id of the bitmap at 0x1FA8, ED3C8 switches on id)
    void                fn_800ED314( u32 id, int arg );
    void                fn_800ED3C8( int id, int arg );
};

extern unk_InboxOwner* lbl_80529D20;    // defined elsewhere, unit not yet reconstructed

#endif
