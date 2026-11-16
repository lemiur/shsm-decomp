// Unit name: the class name strings lbl_803EBCF4 and lbl_803EBD08 ("CPsyPointOfInterest"), read
// by the static initialiser. The inline factory fn_800C7688 and the six thunks follow the
// static initialiser in .text.
//
// Parked: every member is written; fn_801A73A8, fn_800C6D08 and fn_800C6E84 stop short of
// exact (their header comments give the walls).

#include <game/CPsyPointOfInterest.h>
#include <climax/Localisation.h>
#include <climax/Time.h>
#include <climax/unk_801D43E8.h>
#include <game/DisplayHeight.h>
#include <game/PadRumble.h>
#include <game/unk_800276E0.h>
#include <game/unk_800E671C.h>
#include <game/unk_80114598.h>
#include <game/unk_NamedEntryPlayer.h>
#include <game/unk_PromptManager.h>
#include <stdio.h>

u32 lbl_80529AA0 = fn_801A7A84( "CPsyPointOfInterest" );
unk_801A8F6C lbl_80529AA4( "CPsyPointOfInterest", CPsyPointOfInterest::fn_800C7688, sizeof( CPsyPointOfInterest ) );
u32 lbl_80529AA8;
bool lbl_80529AAC;

#define PI                  3.141592f
#define SCREEN_WIDTH        640.0f      // the virtual screen width the centring is measured in
#define PROMPT_SOUND_GAP    10000       // the least time between two prompt sounds (RWS::Time units)
#define FLOAT_MAX           3.4028235e+38f

/*
====================
CPsyPointOfInterest::CPsyPointOfInterest
====================
*/
// 0x800C6508
CPsyPointOfInterest::CPsyPointOfInterest( const unk_AttributePacket& packet ) :
    CPointOfInterest( packet ),
    field_0xBC( false ),
    field_0xC2( true ),
    field_0xC3( true ),
    field_0xC4( false ),
    field_0xC5( false ),
    field_0xC6( false ),
    field_0xC7( false ) {
    matrix = RwFrameGetMatrix( CBaseBehaviour::field_0x30 );
    field_0xBD = true;
    name = NULL;
    field_0xA4 = 1.0f;
    field_0xA8 = 1.0f;
    field_0xA0 = 0.0f;
    field_0x9C = 0.0f;
    field_0xBF = false;
    field_0xC0 = false;
    field_0xC1 = false;
    field_0xBE = true;
    field_0xAC = 0.0f;
    promptName = NULL;
    promptId = 0;
    fn_801A9DE8( lbl_8052A788 );
    fn_801A9DE8( lbl_8052A820, NULL, 0 );
    fn_801A9DE8( lbl_80529D00 );
}

/*
====================
CPsyPointOfInterest::~CPsyPointOfInterest
====================
*/
// 0x800C6648
CPsyPointOfInterest::~CPsyPointOfInterest() {
    fn_801A9940( msgA );
    fn_801A9940( msgB );
    if ( fn_801AA17C( lbl_8052A788 ) ) {
        fn_801A9E00( lbl_8052A788 );
    }
    fn_801A9E00( lbl_8052A820 );
    fn_801A9E00( lbl_80529D00 );
    fn_801A9E00( msgC );
    fn_801A9940( msgC );
    if ( field_0xBC ) {
        lbl_80529C78->fn_800D85CC( promptId );
        field_0xBC = false;
    }
}

/*
====================
CPsyPointOfInterest::fn_801A73A8

Takes the name, the flags, the three messages and the prompt name from the attribute packet;
the prompt string's id is the hash of "<promptName>_Prompt", or 0 when there is no such string.

Parked, wall class register/schedule (95.95%, 32 rows): body, call set and CFG are retail's. The
nonvolatile registers are numbered the other way round (retail: this r30, the jump table base
r31, the flag constants 0/1 in r29/r28; here this r31, table r28, 0/1 in r30/r29), and retail's
SetPromptName arm stores once (li r3, 0 on the empty arm, one stw after the join) where the
if/else here stores on each arm with the cached zero. Tried: the ternary (bool materialised,
worse), resetting text and storing once (merged store, but the CFG inverts: bne for beq/b).
The hash masks with ~0x20 (rlwinm, then extsb), not 0xDF (andi.).
====================
*/
// 0x800C6774
void CPsyPointOfInterest::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    CPointOfInterest::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529AA0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetName:
            name = it->GetString();
            break;
        case CMD_SetField0xBD:
            field_0xBD = it->GetU32() == 0;
            break;
        case CMD_SetField0xA4:
            field_0xA4 = it->GetFloat();
            break;
        case CMD_SetField0xA8:
            field_0xA8 = it->GetFloat();
            break;
        case CMD_SetField0xBE:
            field_0xBE = it->GetU32() != 0;
            break;
        case CMD_SetMsgA:
            fn_801AA1C4( msgA, it->GetString(), 0 );
            break;
        case CMD_SetMsgB:
            fn_801AA1C4( msgB, it->GetString(), 0 );
            break;
        case CMD_SetPromptName: {
            const char* text = it->GetString();
            if ( text != NULL && *text != '\0' ) {
                promptName = text;
            } else {
                promptName = NULL;
            }
            break;
        }
        case CMD_SetField0xC2:
            if ( it->GetU32() != 0 ) {
                field_0xC2 = true;
            } else {
                field_0xC2 = false;
            }
            break;
        case CMD_SetField0xC3:
            if ( it->GetU32() != 0 ) {
                field_0xC3 = true;
            } else {
                field_0xC3 = false;
            }
            break;
        case CMD_SetField0xC4:
            if ( it->GetU32() != 0 ) {
                field_0xC4 = true;
            } else {
                field_0xC4 = false;
            }
            break;
        case CMD_SetMsgC:
            fn_801AA218( msgC, it->GetString(), 0 );
            break;
        case CMD_SetField0xC7:
            if ( it->GetU32() != 0 ) {
                field_0xC7 = true;
            } else {
                field_0xC7 = false;
            }
            break;
        }
        ++it;
    }
    if ( promptName != NULL ) {
        char prompt[0x100];
        sprintf( prompt, "%s_Prompt", promptName );
        const char* p = prompt;
        u32 id = 5381;
        while ( *p != '\0' ) {
            char c = *p++ & ~0x20;
            id = ( id << 5 ) + id + c;
        }
        promptId = id;
        if ( fn_801D2630( promptId, 1 ) == NULL ) {
            promptId = 0;
        }
    }
}

/*
====================
CPsyPointOfInterest::fn_801A7A88

Takes the position and the size of the frame.
====================
*/
// 0x800C69CC
void CPsyPointOfInterest::fn_801A7A88() {
    CAttributeHandler::fn_801A7A88();
    unk_RwMatrix* m = RwFrameGetMatrix( CBaseBehaviour::field_0x30 );
    unk_Vector3 centre = m->pos;
    rect.centre = centre;
    float rightLength = RwV3dLength( &m->right );
    float upLength = RwV3dLength( &m->up );
    rect.width = 2.0f * rightLength;
    rect.height = 2.0f * upLength;
}

/*
====================
CPsyPointOfInterest::fn_8009CB1C

The message handler. Each frame (lbl_8052A788), while the player is in the point's zone and
not in a cut-scene (unless field_0xC4 allows it), updates how long the point has been looked at;
on lbl_80529D00 a point with look time that has not been left counts as looked at; msgC carries a matrix the
point's frame follows (keeping the offset it had on the first one, or copying it when field_0xC7
is set).
====================
*/
// 0x800C6A68
void CPsyPointOfInterest::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788 ) {
        unk_80491B38& record = GetPlayerRecord();
        if ( record.field_0x14 != NULL && ( !record.field_0x14->fn_80043658() || field_0xC4 ) &&
             !record.field_0x14->fn_800631E0() && fn_800C4E8C() == record.field_0x14->GetZone() ) {
            fn_800C6E84( &record );
        }
        return;
    }
    if ( msg.record == lbl_80529D00 ) {
        if ( field_0xA0 > 0.0f && !field_0xC1 && fn_800C54EC() ) {
            fn_800C7468( true );
        }
        return;
    }
    if ( msg.record == msgC ) {
        const unk_RwMatrix* matrix = static_cast< const unk_RwMatrix* >( msg.data );
        if ( !field_0xC6 ) {
            if ( !field_0xC7 ) {
                unk_RwMatrix inverse;
                RwMatrixInvert( &inverse, matrix );
                RwMatrixMultiply( &field_0xD0, RwFrameGetMatrix( CBaseBehaviour::field_0x30 ), &inverse );
            }
            field_0xC6 = true;
        }
        if ( field_0xC7 ) {
            *RwFrameGetMatrix( CBaseBehaviour::field_0x30 ) = *matrix;
        } else {
            RwMatrixMultiply( RwFrameGetMatrix( CBaseBehaviour::field_0x30 ), &field_0xD0, matrix );
        }
        RwFrameUpdateObjects( CBaseBehaviour::field_0x30 );
        rect.centre = *CBaseBehaviour::fn_8009CD08();
        return;
    }
    if ( msg.record == lbl_8052A820 ) {
        // listened to (constructor), but nothing is done with it in this build
        return;
    }
}

/*
====================
CPsyPointOfInterest::fn_800C4E8C
====================
*/
// 0x800C6CB8
CZone* CPsyPointOfInterest::fn_800C4E8C() {
    return CAttributeHandler::mZone;
}


/*
====================
CPsyPointOfInterest::fn_800C54EC

Whether the save-data value is set, unless field_0xC5 is.
====================
*/
// 0x800C6CC0
int CPsyPointOfInterest::fn_800C54EC() {
    bool set = false;
    if ( !field_0xC5 && CPointOfInterest::fn_800C54EC() ) {
        set = true;
    }
    return set;
}

/*
====================
CPsyPointOfInterest::fn_800C6D08

The weight of the point on the screen: the area of the ellipse in the screen rectangle (zero
when its centre is off the screen), times, when field_0xBE is set, how directly the behaviour
faces the camera.

Parked, wall class register/schedule (99.53%, 9 rows): the negated camera at-vector's copy and
scale (toCamera *= -1) take the camera's frame in r5 and the vector's address in r4; retail
numbers them r4 and r5. Tried: a named frame, a named matrix, a const reference to the at
vector, the vector declared or assigned apart, V3dScale written out, the scale through
operator* (temporary: worse). The dot is unk_Vector3::Dot (this first; V3dDot( &a, &b ) numbers
the two addresses the other way round).
====================
*/
// 0x800C6D08
float CPsyPointOfInterest::fn_800C6D08( const unk_ScreenRect* screen, unk_RwCamera* camera ) {
    float x = screen->x;
    float y = screen->y;
    float weight = 0.0f;
    if ( x > 0.0f && x < 1.0f && y > 0.0f && y < 1.0f ) {
        weight = PI * ( 0.5f * screen->width ) * ( 0.5f * screen->height );
    }
    if ( field_0xBE ) {
        unk_Vector3 toCamera = RwFrameGetMatrix( RwCameraGetFrame( camera ) )->at;
        toCamera *= -1.0f;
        unk_Vector3 facing = *CBaseBehaviour::fn_8009CD78();
        V3dNormalize( &facing );
        float facingCamera = toCamera.Dot( facing );
        weight *= facingCamera > 0.0f ? facingCamera : 0.0f;
    }
    return 12.5f * weight;
}

/*
====================
CPsyPointOfInterest::fn_800C6E84

The frame update: when the point is in front of the camera, in its frustum and not hidden by
the zone's collision, measures how centred and how large it is on the screen. While it is
centred, large enough and allowed, shows its prompt; while it counts, adds the frame's look time,
allowing the prompt past field_0xA4 and sending msgA past field_0xA8. A point not looked at loses
its look time when field_0xC3 is set (and counts as left when it had allowed the prompt and
field_0xC2 is set). The prompt stops when the point no longer qualifies for it.

The frame-scope vectors are declared in the reverse of their stack order (MWCC gives the last
declared the lowest slot; this order reproduces retail's frame: toPoint 0x68, sphere 0x78,
cameraPos 0x88, screen 0x98, hit 0xA8).

The camera test is a named bool: retail materialises the negated result (cntlzw/srwi.) before
branching, which MWCC does for a stored value, not for a negation tested in place. The
subtraction from the camera position is the V3dSub call (the operator form numbers its address
registers the other way round).

Parked (98.56%, 114 rows), open walls:
- Register: the player's camera (field_0xC) takes r25 (retail r26), and the first FP block
  numbers the camera position and the near-plane test f6-f9 differently.
- Declaration: retail rounds _rwSqrt's result (frsp), so the caller saw it return a double;
  Gormiti's DWARF and the RW header give float (declared float here: 2 rows).
- Stack/register: inside the occlusion test retail puts end 0x8, start 0x14 and the
  at-vector temporaries above them (0x20-0x5C); here the temporaries are below and the named
  vectors above, and the address registers are numbered differently. Tried: swapping the
  declarations and the two updates, declaring start and end at frame scope.
- Register: FP numbering of the field_0x9C/field_0x1508 compares and the look-time update
  (f6/f7 swapped).
====================
*/
// 0x800C6E84
void CPsyPointOfInterest::fn_800C6E84( unk_80491B38* player ) {
    unk_LineHit hit;
    unk_ScreenRect screen;
    unk_Vector3 cameraPos;
    unk_RwSphere sphere;
    unk_Vector3 toPoint;
    bool notLookedAt = true;
    bool stopPrompt = true;
    unk_RwCamera* camera = player->field_0xC;
    const unk_RwMatrix* view = RwFrameGetLTM( RwCameraGetFrame( camera ) );
    toPoint = *CBaseBehaviour::fn_8009CD08();
    cameraPos = view->pos;
    V3dSub( &toPoint, &toPoint, &cameraPos );
    bool inView;
    if ( V3dDot( &toPoint, &view->at ) <= camera->nearPlane * camera->nearPlane ) {
        inView = false;
    } else {
        sphere.center = rect.centre;
        sphere.radius = 0.1f;
        inView = RwCameraFrustumTestSphere( camera, &sphere ) != 0;
    }
    if ( inView ) {
        unk_RwFrame* cameraFrame = RwCameraGetFrame( player->field_0xC );
        bool hidden = false;
        u32 world = fn_800276D0( fn_800C4E8C() );
        if ( world != 0 ) {
            unk_Vector3 end = rect.centre;
            unk_Vector3 start = RwFrameGetMatrix( cameraFrame )->pos;
            end -= unk_Vector3( RwFrameGetMatrix( cameraFrame )->at ) * 0.3f;
            start += unk_Vector3( RwFrameGetMatrix( cameraFrame )->at ) * 0.1f;
            if ( fn_801D43E8( world, &start, &end, &hit, true, 0xE ) ) {
                hidden = true;
            }
        }
        if ( !hidden && fn_801F7E80( &rect, player->field_0xC, &screen ) ) {
            float x = SCREEN_WIDTH * screen.x;
            float y = screen.y * GetDisplayHeight();
            float centreX;
            float centreY;
            bool fromAimPoint = !lbl_80529E44->GetCamera()->fn_800B5BD8();
            if ( fromAimPoint ) {
                centreX = fn_800067B4()->field_0x28;
                centreY = fn_800067B4()->field_0x2C;
            } else {
                centreX = 320.0f;
                centreY = GetDisplayHeight() / 2;
            }
            float dx = x - centreX;
            float dy = y - centreY;
            float distance = ( dx * dx + dy * dy ) / ( SCREEN_WIDTH * SCREEN_WIDTH );
            if ( lbl_80529AAC ) {
                field_0x9C = 1.0f - distance;
            } else {
                field_0x9C = 1.0f - _rwSqrt( distance );
            }
            if ( field_0x9C < 0.0f ) {
                field_0x9C = 0.0f;
            }
            float weight = fn_800C6D08( &screen, player->field_0xC );
            if ( promptId != 0 && field_0xBF && field_0x9C > player->field_0x14->field_0x1508 && weight >= 0.35f ) {
                if ( !field_0xBC && ( fn_801D2014() || lbl_80529C78->fn_800D8DBC() ) ) {
                    lbl_80529C78->fn_800D84C4( promptId, 0, true, 0.0f );
                    field_0xBC = true;
                }
                stopPrompt = false;
            }
            if ( fn_800C54EC() ) {
                if ( !field_0xC0 ) {
                    field_0xAC = 0.0f;
                    if ( weight >= 0.01f ) {
                        if ( field_0x9C > player->field_0x14->field_0x1508 ) {
                            field_0xA0 += player->field_0x8 * ( weight * field_0x9C );
                            field_0xAC = weight;
                            if ( field_0xA0 > field_0xA4 * player->field_0x14->field_0x1500 ) {
                                field_0xBF = true;
                            }
                            if ( field_0xA0 > field_0xA8 * player->field_0x14->field_0x1504 ) {
                                field_0xC0 = true;
                                fn_800C7468( true );
                            }
                        }
                        notLookedAt = false;
                    }
                }
                if ( notLookedAt && field_0xC3 ) {
                    field_0xA0 = 0.0f;
                    if ( field_0xBF && field_0xC2 ) {
                        field_0xC1 = true;
                        fn_800C7468( false );
                    }
                }
            }
        }
    }
    if ( stopPrompt && field_0xBC ) {
        lbl_80529C78->fn_800D85CC( promptId );
        field_0xBC = false;
    }
}

/*
====================
CPsyPointOfInterest::fn_800C7468

Marks the point used and plays its named entry. On entering, when the entry played and the
player allows it, the prompt sound plays when the point filled more than half its ellipse and
the last one is long enough ago; then msgA is sent on entering, msgB on leaving.
====================
*/
// 0x800C7468
void CPsyPointOfInterest::fn_800C7468( bool entering ) {
    field_0xC5 = true;
    unk_NamedEntryResult result;
    lbl_8052A0D0->fn_8014EC98( name, entering, &result );
    if ( ( result.field_0x0 || result.field_0x1 ) && entering ) {
        u32 now = RWS::Time::GetTime();
        u32 elapsed = now - lbl_80529AA8;
        CPlayer* player = GetPlayerRecord().field_0x14;
        if ( player != NULL && player->fn_8007046C() ) {
            float area = PI * ( 0.5f * rect.width ) * ( 0.5f * rect.height );
            float filled = area > 0.0f ? field_0xAC / area : FLOAT_MAX;
            if ( filled > 0.5f && elapsed > PROMPT_SOUND_GAP ) {
                lbl_80529AA8 = now;
                player->fn_80070490( promptName, NULL, true );
            }
        }
    }
    if ( entering ) {
        PostMsg( msgA, NULL );
    } else {
        PostMsg( msgB, NULL );
    }
}

// Compiler-generated, no source of their own: __sinit_CPsyPointOfInterest_cpp (0x800C75FC).
