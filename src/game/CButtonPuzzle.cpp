// unit name unconfirmed: the class name is the registration string "CButtonPuzzle"; the file name
// is invented. The inline slots of unk_80161958 are emitted among CButtonPuzzle's own functions,
// and the inline factory fn_8015AFB4 and the thunks follow the static initialiser in .text.

#include <game/CButtonPuzzle.h>
#include <game/DisplayHeight.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>

u32 lbl_8052A170 = fn_801A7A84( "CButtonPuzzle" );
unk_801A8F6C lbl_8052A174( "CButtonPuzzle", CButtonPuzzle::fn_8015AFB4, sizeof( CButtonPuzzle ) );

/* attribute command ids of CButtonPuzzle (names invented) */
enum {
    CMD_SetFadeOut  = 0,
    CMD_SetFadeTime = 1
};

/*
====================
CButtonPuzzle::fn_801A73A8
====================
*/
// 0x8015A830
void CButtonPuzzle::fn_801A73A8( const unk_AttributePacket& packet ) {
    unk_80161958::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_8052A170 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetFadeOut:
            if ( it->GetU32() != 0 ) {
                field_0x5BD = true;
            } else {
                field_0x5BD = false;
            }
            break;
        case CMD_SetFadeTime:
            field_0x5C0 = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
CButtonPuzzle::fn_80164578
====================
*/
// 0x8015A8F8
void CButtonPuzzle::fn_80164578() {
    field_0x5BC = fn_80160E10( fn_800067B4() );
    fn_801606BC( 0.0f );
    fn_8015A9A8( 0.0f );
    fn_80024800();
    fn_80160748( 0.0f );
    unk_80161958::fn_80164578();
    if ( !field_0x4E5 ) {
        fn_801DD29C( field_0x130, 255, 0 );
    }
}

/*
====================
CButtonPuzzle::fn_8009CB1C

Counts the fade down on the frame message and runs the press logic.
NonMatching: register residue, tweaks and record take r30 and r29 where retail has r29 and r30
(every declaration order and form tried, const tweaks or record, record as a pointer or as
GetPlayerRecord() at each use, record before the base call or after the message test, tweaks
declared at its assignment: those move this off r31 or the address load below the test; wrapping
the body after the base call in an inline helper with state, tweaks, record reverses local order
and matches 0/201, confirming this is purely MWCC's Simplify tie-break, left un-wrapped per R17).
Also ruled out: the event id conversion in the message test (the cause in CActorTest and
CCollisionCapsule; neutral here), record bound to lbl_80491B38 directly, state declared at the
top, -ipa file dropped or -O4,p. The PS2 build calls the tweaks getter a second time for the last
fn_80160E10; Wii reuses tweaks, so the PS2 shape does not decide the local census.
====================
*/
// 0x8015AA80
void CButtonPuzzle::fn_8009CB1C( unk_Msg& msg ) {
    unk_80161958::fn_8009CB1C( msg );
    unk_Tweaks* tweaks;
    unk_80491B38& record = GetPlayerRecord();
    if ( msg.record != lbl_8052A788 ) {
        return;
    }

    if ( field_0x5C4 != 0.0f ) {
        field_0x5C4 -= record.field_0x8;
        if ( field_0x5C4 < 0.0f ) {
            field_0x5C4 = 0.0f;
        }
        fn_801DD29C( field_0x130, ( int )( 255.0f * ( field_0x5C4 / field_0x5C0 ) ), 0 );
    }

    if ( !field_0x4E4 ) {
        return;
    }

    tweaks = fn_800067B4();
    if ( fn_80160ECC( tweaks ) && fn_8015F62C() && !field_0x4EC ) {
        fn_80164894();
        return;
    }

    u32 state = fn_801650EC();
    if ( field_0x5B8 == 0 ) {
        if ( tweaks->field_0x30 && fn_80160E10( tweaks ) && fn_80160DDC() ) {
            // NOTE: dead store: retail writes this pair to the stack and never reads it (without
            // it the two stores vanish); guess: the press range the other puzzles pass to fn_80165134
            unk_RwV2d range = { tweaks->field_0x28, tweaks->field_0x2C };
            if ( state == 1 ) {
                fn_80164BE0();
            }
        }
    } else if ( field_0x5B8 == 2 ) {
        if ( tweaks->field_0x30 && !fn_80160E10( tweaks ) ) {
            fn_80164C54();
        } else if ( !tweaks->field_0x30 ) {
            fn_80164C54();
        }
        if ( state == 0 ) {
            fn_80164C54();
        }
    }

    if ( fn_80160E10( tweaks ) && !field_0x5BC && ( fn_80160DDC() || field_0x5B8 == 2 )
         && ( !field_0x4E5 || field_0x4E9 ) && state != 0 && fn_8015F62C() ) {
        fn_801606BC( fn_80160584( 0.0f ) );
        fn_80164F08( 0.0f, 1.0f, record.field_0x8 );
        if ( field_0x4E6 ) {
            fn_80164894();
        }
        if ( !field_0x4E9 ) {
            field_0x168 |= 0x20;
        }
        if ( field_0x5BD ) {
            field_0x5C4 = field_0x5C0;
        }
        field_0x4E5 = true;
    } else {
        fn_80165AFC( state );
    }
    field_0x5BC = fn_80160E10( tweaks );
}

/*
====================
CButtonPuzzle::fn_80165B90
====================
*/
// 0x8015ADA4
void CButtonPuzzle::fn_80165B90( bool reset ) {
    unk_80161958::fn_80165B90( reset );
    if ( reset ) {
        field_0x5C4 = 0.0f;
    }
}

/*
====================
CButtonPuzzle::fn_8015DBD4
====================
*/
// 0x8015ADEC
void CButtonPuzzle::fn_8015DBD4() {
    if ( field_0x5BD ) {
        field_0x5C4 = field_0x5C0;
    } else {
        unk_80161958::fn_8015DBD4();
    }
}

/*
====================
CButtonPuzzle::fn_8015AE1C
====================
*/
// 0x8015AE1C
u32 CButtonPuzzle::fn_8015AE1C( u32* out ) {
    if ( ( !field_0x4E5 || field_0x4E9 ) && fn_801650EC() ) {
        *out = 1;
        return 3;
    }
    *out = 0;
    if ( field_0x250 != 10 ) {
        return 2;
    }
    return field_0x250;
}
