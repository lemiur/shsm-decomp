// unit name unconfirmed: the class name is the registration string "CDrawingTest"; the file name
// is invented. CCrayon's functions and registration follow CDrawingTest's in .text and in the
// static initialiser; the inline factories and the thunks follow the static initialiser.
// NonMatching: five functions short (fn_801A7A88 loop shape, fn_801785B4/fn_80179CC0 register
// allocation, fn_80179304/fn_801796F8 scheduling) and three R18 paired-single helpers (MATCH-PENDING).

#include <game/CDrawingTest.h>
#include <climax/RenderState.h>
#include <game/DisplayHeight.h>
#include <game/unk_800276E0.h>
#include <game/unk_MessageInbox.h>
#include <game/Display.h>
#include <game/PadRumble.h>
#include <game/SaveData.h>
#include <game/unk_8000A040.h>
#include <game/unk_80491B38.h>
#include <game/unk_HUDController.h>
#include <game/unk_NamedEntryPlayer.h>
#include <game/VectorPS.h>
#include <math.h>
#include <stdio.h>

/*
 * Tables of fn_801785B4. They are defined ahead of the code: their strings precede every
 * literal of the functions in .data.
 */
const char* lbl_80528500[2] = { "SaveData", NULL };
const char* const lbl_8052DBD0 = "PaintTest_";
const char* const lbl_803D9250[8] = {
    "Nothing", "Wall", "Roof", "Mailbox", "MrsLevin", "MrLevin", "Car", "Door"
};

u32 lbl_8052A338 = fn_801A7A84( "CDrawingTest" );
unk_801A8F6C lbl_8052A33C( "CDrawingTest", CDrawingTest::fn_8017A270, sizeof( CDrawingTest ) );
CDrawingTest* lbl_8052A340;
CCrayon* lbl_804780C0[12];
u32 lbl_8052A344 = fn_801A7A84( "CCrayon" );
unk_801A8F6C lbl_8052A348( "CCrayon", CCrayon::fn_8017A2B8, sizeof( CCrayon ) );

/*
====================
V3dScale

Scales v by s in place; paired-single code. Name invented.
====================
*/
static inline void V3dScale( register unk_Vec3* v, register float s ) {
    asm {
        psq_l       fp1, 8(v), 1, 0
        psq_l       fp0, 0(v), 0, 0
        ps_muls0    fp3, fp1, s
        ps_muls0    fp2, fp0, s
        psq_st      fp3, 8(v), 1, 0
        psq_st      fp2, 0(v), 0, 0
    }
}

/*
====================
CDrawingTest::CDrawingTest
====================
*/
// 0x80177864
CDrawingTest::CDrawingTest( const unk_AttributePacket& packet ) :
    CBasicAnimatedObject( packet ),
    field_0x408( NULL ),
    field_0x40C( NULL ),
    field_0x430( 60.0f ),
    field_0x434( NULL ),
    field_0x438( 5 ),
    field_0x43C( NULL ),
    field_0x440( 8 ),
    field_0x444( NULL ),
    field_0x448( 50.0f ),
    field_0x44C( NULL ),
    field_0x450( 50 ),
    field_0x454( NULL ),
    field_0x474( 1.0f ),
    field_0x4B9( true ) {
    lbl_8052A340 = this;
    memset( lbl_804780C0, 0, sizeof( lbl_804780C0 ) );
    memset( field_0x478, 0, sizeof( field_0x478 ) );

    field_0x3FC = RwImageCreate( 256, 256, 32 );
    field_0x400 = true;
    field_0x3FC = RwImageAllocatePixels( field_0x3FC );
    field_0x404 = RwRasterCreate( 256, 256, 32, rwRASTERTYPETEXTURE );
    if ( field_0x3FC->cpPixels != NULL ) {
        memset( field_0x3FC->cpPixels, 0xFF, 256 * 256 * 4 );
    }
    field_0x404 = RwRasterSetFromImage( field_0x404, field_0x3FC );

    field_0x414 = -1;
    field_0x418 = -1;
    field_0x4B4 = false;
    field_0x4B5 = false;
    field_0x4B6 = false;
    field_0x4B7 = false;
    field_0x4B8 = false;
    field_0x41C.x = 0.0f;
    field_0x41C.y = 0.0f;
    field_0x424.x = 0.0f;
    field_0x424.y = 0.0f;
    field_0x42C = 0;
    field_0x468 = 0;
    field_0x410 = 0;
    field_0x458 = 0.0f;
    field_0x45C = 0;
    field_0x460 = 0;
    field_0x464 = 0.0f;
    field_0x46C.red = 0;
    field_0x46C.green = 0;
    field_0x46C.blue = 0;
    fn_801A9DE8( lbl_8052A788 );
}

/*
====================
CDrawingTest::~CDrawingTest
====================
*/
// 0x80177AAC
CDrawingTest::~CDrawingTest() {
    lbl_80529D20->field_0x6C4.fn_800E588C( 0x33, 1, 0 );
    lbl_80529D20->fn_800E97C8( this );
    lbl_8052A340 = NULL;
    fn_801A9E00( lbl_8052A788 );
    RwTextureDestroy( field_0x408 );
    RwTextureDestroy( field_0x40C );
    RwRasterDestroy( field_0x404 );
    RwImageDestroy( field_0x3FC );
    fn_801A9E00( field_0x3D4 );
    fn_801A9940( field_0x3D4 );
    fn_801A9E00( field_0x3E4 );
    fn_801A9940( field_0x3E4 );
    fn_801A9940( field_0x3DC );
    fn_801A9940( field_0x3EC );
    fn_801A9940( field_0x3F4 );
    fn_801A9940( field_0x4BC );
    fn_801A9940( field_0x4C4 );
}

/*
====================
CDrawingTest::fn_801A73A8
====================
*/
// 0x80177C44
void CDrawingTest::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBasicAnimatedObject::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_8052A338 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetListenMsg0x3D4:
            fn_801AA218( field_0x3D4, it->GetString(), NULL );
            break;
        case CMD_SetSendMsg0x3DC:
            fn_801AA1C4( field_0x3DC, it->GetString(), NULL );
            break;
        case CMD_SetListenMsg0x3E4:
            fn_801AA218( field_0x3E4, it->GetString(), NULL );
            break;
        case CMD_SetSendMsg0x3EC:
            fn_801AA1C4( field_0x3EC, it->GetString(), NULL );
            break;
        case CMD_SetSendMsg0x3F4:
            fn_801AA1C4( field_0x3F4, it->GetString(), NULL );
            break;
        case CMD_SetField0x430:
            field_0x430 = it->GetFloat();
            break;
        case CMD_SetField0x434:
            field_0x434 = it->GetString();
            break;
        case CMD_SetField0x438:
            field_0x438 = it->GetU32();
            break;
        case CMD_SetField0x43C:
            field_0x43C = it->GetString();
            break;
        case CMD_SetField0x440:
            field_0x440 = it->GetU32();
            break;
        case CMD_SetField0x444:
            field_0x444 = it->GetString();
            break;
        case CMD_SetField0x448:
            field_0x448 = it->GetFloat();
            break;
        case CMD_SetField0x44C:
            field_0x44C = it->GetString();
            break;
        case CMD_SetField0x450:
            field_0x450 = it->GetU32();
            break;
        case CMD_SetField0x454:
            field_0x454 = it->GetString();
            break;
        case CMD_SetSendMsg0x4BC:
            fn_801AA1C4( field_0x4BC, it->GetString(), NULL );
            break;
        case CMD_SetSendMsg0x4C4:
            fn_801AA1C4( field_0x4C4, it->GetString(), NULL );
            break;
        }
        ++it;
    }
}

/*
 * True when the pointer is level with the screen position pos (within 12 lines) and right of
 * it. Name invented.
 */
static inline bool IsPointerAt( const unk_RwV3d& pos ) {
    unk_Tweaks* tweaks = fn_800067B4();
    float x = tweaks->field_0x28;
    float y = tweaks->field_0x2C;
    if ( y > pos.y - 12.0f && y < 12.0f + pos.y && x <= pos.x ) {
        return true;
    }
    return false;
}

/* The index of the crayon of the given colour in lbl_804780C0, or -1. Name invented. */
static inline int FindCrayon( unk_RGBA colour ) {
    for ( int i = 0; i < 12; i++ ) {
        if ( lbl_804780C0[i]->colour == colour ) {
            return i;
        }
    }
    return -1;
}

/*
====================
CDrawingTest::fn_8009CB1C

The message handler. Each frame (lbl_8052A788) while painting: picks the crayon under the
pointer (the crayons' positions projected to the screen) and takes it up on a press, paints
(fn_801796F8), and ends the painting when the button is held (field_0x3DC). field_0x3D4 starts
the painting; field_0x3E4 shows the yes/no prompt (HUD element 0x33), which a pointer shaken
side to side (field_0x3F4) or up and down (field_0x3EC) answers.
====================
*/
// 0x80177DEC
void CDrawingTest::fn_8009CB1C( unk_Msg& msg ) {
    CBasicAnimatedObject::fn_8009CB1C( msg );
    unk_Tweaks* tweaks = fn_800067B4();
    if ( msg.record == lbl_8052A788.record ) {
        unk_80491B38& record = GetPlayerRecord();
        fn_80179304( record.field_0xC );
        if ( !field_0x4B5 && field_0x4B6 ) {
            field_0x458 += record.field_0x8;
            field_0x414 = -1;
            if ( !field_0x4B4 ) {
                for ( u32 i = 0; i < 12; i++ ) {
                    if ( i != field_0x418 ) {
                        unk_RwV3d screen;
                        RwV3dTransformPoint( &screen, lbl_804780C0[i]->fn_8009CD08(), &record.field_0xC->viewMatrix );
                        screen.x /= screen.z;
                        screen.y /= screen.z;
                        screen.x *= 640.0f;
                        screen.y *= GetDisplayHeight();
                        if ( IsPointerAt( screen ) ) {
                            field_0x414 = i;
                        }
                    }
                }
            }
            if ( field_0x414 >= 0 && field_0x414 != field_0x418 ) {
                lbl_80529D20->fn_800E97C0( this, 4, false, false );
            } else if ( field_0x418 == -1 ) {
                lbl_80529D20->fn_800E97C0( this, 2, false, false );
            } else {
                lbl_80529D20->fn_800E97C0( this, 13, false, false );
            }
            if ( field_0x414 >= 0 && field_0x414 != field_0x418 && tweaks->field_0x56 ) {
                field_0x418 = field_0x414;
                unk_RGBA colour = lbl_804780C0[field_0x414]->colour;
                field_0x46C.red = colour.red;
                field_0x46C.green = colour.green;
                field_0x46C.blue = colour.blue;
                field_0x46C.alpha = colour.alpha;
                field_0x470 = lbl_804780C0[field_0x414]->size;
                field_0x414 = -1;
                field_0x460++;
            }
            fn_801796F8( record.field_0xC );
            if ( tweaks->field_0x23 && !field_0x4B9 && !field_0x4B4 ) {
                field_0x414 = -1;
                field_0x418 = -1;
                lbl_80529D20->field_0x6C4.fn_800E5490( 3, 0 );
                PostMsg( field_0x3DC, NULL );
                field_0x4B5 = true;
                fn_8000BEA0();
                fn_8000A028( false );
                lbl_80529D20->fn_800E97C8( this );
            }
            field_0x4B9 = tweaks->field_0x23;
        }
        if ( field_0x4B7 ) {
            float dx = tweaks->field_0x28 - field_0x49C;
            float dy = tweaks->field_0x2C - field_0x4A0;
            if ( field_0x4A4 > 0.0f && dx > field_0x4A4 ) {
                field_0x4A4 *= -1.0f;
                field_0x4AC++;
            } else if ( field_0x4A4 < 0.0f && dx < field_0x4A4 ) {
                field_0x4A4 *= -1.0f;
                field_0x4AC++;
            }
            if ( field_0x4A8 > 0.0f && dy > field_0x4A8 ) {
                field_0x4A8 *= -1.0f;
                field_0x4B0++;
            } else if ( field_0x4A8 < 0.0f && dy < field_0x4A8 ) {
                field_0x4A8 *= -1.0f;
                field_0x4B0++;
            }
            if ( field_0x4AC > 2 ) {
                field_0x4B7 = false;
                PostMsg( field_0x3F4, NULL );
                lbl_80529D20->field_0x6C4.fn_800E588C( 0x33, 1, 0 );
            }
            if ( field_0x4B0 > 2 ) {
                field_0x4B7 = false;
                PostMsg( field_0x3EC, NULL );
                lbl_80529D20->field_0x6C4.fn_800E588C( 0x33, 1, 0 );
            }
        }
    } else if ( msg.record == field_0x3D4.record ) {
        field_0x4B5 = false;
        field_0x4B6 = true;
        field_0x4B9 = true;
        fn_8000BED8();
        fn_8000A028( true );
        lbl_80529D20->field_0x6C4.fn_800E50E8( "ui_hud_paint_prompt", 3, NULL );
    } else if ( msg.record == field_0x3E4.record ) {
        unk_HUDElement* element = lbl_80529D20->field_0x6C4.fn_800E4148( 0x33, "Yes", 1, 0, 1.0f );
        element->PlayPanelTransition( "No", 1, 0, 0, 1.0f );
        field_0x4B7 = true;
        field_0x49C = tweaks->field_0x28;
        field_0x4A0 = tweaks->field_0x2C;
        field_0x4A4 = 10.0f;
        field_0x4A8 = 10.0f;
        field_0x4AC = 0;
        field_0x4B0 = 0;
    }
}

/*
====================
CDrawingTest::fn_801A7A88

Loads the two textures and copies the outline map's region ids into the alpha bytes of the
drawing.
NonMatching: loop shape, retail keeps x * 4 as an offset added to the pixels per byte and unrolls
the inner loop by 4; every spelling tried makes a pointer induction variable unrolled by 8.
====================
*/
// 0x801783A4
void CDrawingTest::fn_801A7A88() {
    CBasicAnimatedObject::fn_801A7A88();
    field_0x408 = FindTexture( "PR_PSY02_HouseDrawing", 1 );
    field_0x40C = FindTexture( "PR_PaintingOutlineIDMap", 1 );

    RwRaster* map = field_0x40C->raster;
    u8* ids = RwRasterLock( map, 0, rwRASTERLOCKREAD );
    s32 mapStride = map->stride;
    u8* pixels = field_0x3FC->cpPixels;
    field_0x410 = field_0x3FC->stride;
    for ( u32 y = 0; y < 256; y++ ) {
        for ( u32 x = 0; x < 256; x++ ) {
            pixels[y * field_0x410 + x * 4 + 3] = ids[y * mapStride + x];
        }
    }
    RwRasterUnlock( map );
}

/*
====================
CDrawingTest::fn_8000CFCC
====================
*/
// 0x801784D8
void CDrawingTest::fn_8000CFCC() {
    fn_801785B4();
}

/*
====================
CDrawingTest::fn_8000CFD0

Creates the atomic that draws the drawing and adds it to the world.
====================
*/
// 0x801784DC
void CDrawingTest::fn_8000CFD0() {
    field_0x498 = RpAtomicCreate();
    RpAtomicSetRenderCallBack( field_0x498, fn_80178580 );
    RpAtomicSetFrame( field_0x498, field_0x30 );
    RpAtomicSetFlags( field_0x498, ATOMIC_RENDER );
    unk_RwSphere* sphere = RpAtomicGetBoundingSphere( field_0x498 );
    if ( sphere != NULL ) {
        sphere->radius = 1.0f;
    }
    fn_800276E0( mZone, field_0x498 );
}

/*
====================
CDrawingTest::fn_80178580
====================
*/
// 0x80178580
unk_RpAtomic* CDrawingTest::fn_80178580( unk_RpAtomic* atomic ) {
    lbl_8052A340->fn_80178A60();
    return atomic;
}

/*
====================
CDrawingTest::fn_801785B4

Scores the drawing: counts the painted pixels and the crayons used, files each region's
crayon pixels, saves the crayon that painted most of each region ("PaintTest_<region>" in
"SaveData") and reports the five results to lbl_8052A0D0.
NonMatching: register/schedule (wall.exe: no consistent renaming), two loops: the pixel loop
swaps r3/r4 between the next free slot of crayons and the x * 4 offset, the region loop numbers
its five registers in another order; every declaration order and loop form tried.
====================
*/
// 0x801785B4
void CDrawingTest::fn_801785B4() {
    unk_NamedEntryPlayer* player;
    u8* pixels = field_0x3FC->cpPixels;
    int painted = 0;
    int numCrayons = 0;
    int crayons[12];
    memset( crayons, -1, sizeof( crayons ) );
    int* next = crayons;
    for ( u32 y = 0; y < 256; y++ ) {
        for ( u32 x = 0; x < 256; x++ ) {
            unk_RGBA colour;
            u32 offset = y * field_0x410 + x * 4;
            colour.alpha = 255;
            colour.red = pixels[offset];
            colour.green = pixels[offset + 1];
            colour.blue = pixels[offset + 2];
            int region = pixels[offset + 3];
            if ( region >= 8 ) {
                region = 0;
            }
            if ( colour.red != 255 || colour.green != 255 || colour.blue != 255 ) {
                painted++;
                int crayon = FindCrayon( colour );
                bool found = false;
                for ( int i = 0; i < numCrayons; i++ ) {
                    if ( crayon == crayons[i] ) {
                        found = true;
                    }
                }
                if ( !found ) {
                    *next++ = crayon;
                    numCrayons++;
                }
                if ( region != 0 ) {
                    field_0x234[region].crayonPixels[crayon]++;
                }
            }
        }
    }
    field_0x45C = numCrayons;
    field_0x464 = painted / 65536.0f;
    field_0x464 *= 100.0f;

    for ( int region = 1; region < 8; region++ ) {
        field_0x234[region].crayon = 0;
        u32 most = 5;
        for ( int crayon = 0; crayon < 12; crayon++ ) {
            if ( field_0x234[region].crayonPixels[crayon] > most ) {
                field_0x234[region].crayon = crayon;
                most = field_0x234[region].crayonPixels[crayon];
            }
        }
    }

    for ( int region = 1; region < 8; region++ ) {
        char name[64];
        sprintf( name, "%s%s", lbl_8052DBD0, lbl_803D9250[region] );
        unk_PaintRegion& paint = field_0x234[region];
        void* save = fn_8020609C( lbl_80528500[0], NULL, 1 );
        if ( save != NULL ) {
            u32 crayon = paint.crayon;
            fn_80205354( save, name, 3, &crayon, sizeof( crayon ) );
        }
    }

    player = lbl_8052A0D0;
    if ( field_0x434 != NULL && field_0x434[0] != '\0' ) {
        player->fn_8014EC98( field_0x434, field_0x458 > field_0x430, NULL );
    }
    if ( field_0x43C != NULL && field_0x43C[0] != '\0' ) {
        player->fn_8014EC98( field_0x43C, field_0x45C > field_0x438, NULL );
    }
    if ( field_0x444 != NULL && field_0x444[0] != '\0' ) {
        player->fn_8014EC98( field_0x444, field_0x460 > field_0x440, NULL );
    }
    if ( field_0x44C != NULL && field_0x44C[0] != '\0' ) {
        player->fn_8014EC98( field_0x44C, field_0x464 >= field_0x448, NULL );
    }
    if ( field_0x454 != NULL && field_0x454[0] != '\0' ) {
        player->fn_8014EC98( field_0x454, field_0x468 > field_0x450, NULL );
    }
}

/*
====================
CDrawingTest::fn_80178A60

Draws the drawing as a quad at the behaviour's frame: refreshes the raster from the image when it
changed, draws the painted image opaque, then multiplies the house drawing over it (at half
brightness while lbl_80527E50 is set), and restores the render states.
MATCH-PENDING (R18): V3dScale is paired-single inline asm (C cannot express it).
====================
*/
// 0x80178A60
void CDrawingTest::fn_80178A60() {
    if ( !field_0x4B5 && field_0x400 ) {
        field_0x404 = RwRasterSetFromImage( field_0x404, field_0x3FC );
        field_0x400 = false;
    }

    RwImVertexIndex indices[6];
    unk_Vec3 pos;
    unk_Vec3 at;
    unk_Vec3 right;
    unk_Vec3 corner0;
    unk_Vec3 corner1;
    unk_Vec3 corner2;
    unk_Vec3 corner3;
    unk_RwIm3DVertex vertices[4];

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 2;
    indices[4] = 3;
    indices[5] = 0;
    fn_8009CCE8( &pos );
    fn_8009CD84( &at );
    fn_8009CD2C( &right );

    unk_Vec3 offset( right );
    V3dScale( &offset, 0.05f );
    pos.x += offset.x;
    pos.y += offset.y;
    pos.z += offset.z;
    at.x = 0.1f * at.x;
    at.y = 0.1f * at.y;
    at.z = 0.1f * at.z;
    right.x = 0.13333334f * right.x;     // 0.1 * 4 / 3
    right.y = 0.13333334f * right.y;
    right.z = 0.13333334f * right.z;

    corner0.x = pos.x - at.x;
    corner0.y = pos.y - at.y;
    corner0.z = pos.z - at.z;
    corner0.x -= right.x;
    corner0.y -= right.y;
    corner0.z -= right.z;
    corner1.x = pos.x + at.x;
    corner1.y = pos.y + at.y;
    corner1.z = pos.z + at.z;
    corner1.x -= right.x;
    corner1.y -= right.y;
    corner1.z -= right.z;
    corner2.x = pos.x + at.x;
    corner2.y = pos.y + at.y;
    corner2.z = pos.z + at.z;
    corner2.x += right.x;
    corner2.y += right.y;
    corner2.z += right.z;
    corner3.x = pos.x - at.x;
    corner3.y = pos.y - at.y;
    corner3.z = pos.z - at.z;
    corner3.x += right.x;
    corner3.y += right.y;
    corner3.z += right.z;

    vertices[0].SetPos( corner0 );
    vertices[1].SetPos( corner1 );
    vertices[2].SetPos( corner2 );
    vertices[3].SetPos( corner3 );
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;
    vertices[1].u = 0.0f;
    vertices[1].v = 1.0f;
    vertices[2].u = 1.0f;
    vertices[2].v = 1.0f;
    vertices[3].u = 1.0f;
    vertices[3].v = 0.0f;

    u8 level = lbl_80527E50 ? 127 : 255;
    for ( int i = 0; i < 4; i++ ) {
        vertices[i].color.red = 255;
        vertices[i].color.green = 255;
        vertices[i].color.blue = 255;
        vertices[i].color.alpha = 255;
    }
    RwRenderStateSet( rwRENDERSTATETEXTURERASTER, field_0x404 );
    u32 oldCull = lbl_80528870;
    SetCullMode( 0 );
    SetBlendMode( BLENDMODE_OPAQUE );
    SetDepthMode( DEPTHMODE_TESTONLY );
    RwRenderStateSet( rwRENDERSTATETEXTUREADDRESS, ( void* )rwTEXTUREADDRESSWRAP );
    RwRenderStateSet( rwRENDERSTATETEXTUREFILTER, ( void* )rwFILTERLINEAR );
    RwIm3DTransform( vertices, 4, NULL, IM3D_VERTEXXYZ | IM3D_VERTEXRGBA | IM3D_VERTEXUV );
    RwIm3DRenderIndexedPrimitive( rwPRIMTYPETRILIST, indices, 6 );

    RwRaster* houseDrawing = field_0x408->raster;
    for ( int i = 0; i < 4; i++ ) {
        vertices[i].color.red = level;
        vertices[i].color.green = level;
        vertices[i].color.blue = level;
        vertices[i].color.alpha = 255;
    }
    RwRenderStateSet( rwRENDERSTATETEXTURERASTER, houseDrawing );
    SetBlendMode( BLENDMODE_MULTIPLY );
    RwIm3DRenderIndexedPrimitive( rwPRIMTYPETRILIST, indices, 6 );
    RwIm3DEnd();

    SetBlendMode( BLENDMODE_ALPHA );
    SetDepthMode( DEPTHMODE_ON );
    SetCullMode( oldCull );
}

/*
====================
CDrawingTest::fn_80179304

Fans the crayons out in front of the behaviour, each one turned to face it: the crayon in hand
(field_0x418) is skipped, the selected one (field_0x414) is lifted and drawn larger. camera is
unused (the caller passes it).
NonMatching: register/schedule; the paired-single temporaries and their copies are retail's, the
order MWCC schedules them in is not (also MATCH-PENDING R18: the vector helpers are inline asm).
====================
*/
// 0x80179304
void CDrawingTest::fn_80179304( unk_RwCamera* camera ) {
    if ( lbl_804780C0[0] == NULL ) {
        return;
    }
    unk_Vector3 position = *fn_8009CD08();
    unk_Vector3 right = *fn_8009CD20();
    unk_Vector3 at = *fn_8009CD78();
    unk_Vector3 up = *fn_8009CD4C();
    unk_Vector3 scale( 0.5f, 0.5f, 0.5f );
    for ( u32 i = 0; i < 12; i++ ) {
        if ( i == field_0x418 ) {
            continue;
        }
        unk_RwMatrix matrix;
        lbl_804780C0[i]->fn_8009CDA4( &matrix );
        unk_Vector3 point = position;
        point += right * -0.1f;
        point += at * ( i - 5.5f ) * 0.015f;
        matrix.right.x = -1.0f * at.x;
        matrix.right.y = -1.0f * at.y;
        matrix.right.z = -1.0f * at.z;
        matrix.up.x = up.x;
        matrix.up.y = up.y;
        matrix.up.z = up.z;
        matrix.at.x = right.x;
        matrix.at.y = right.y;
        matrix.at.z = right.z;
        if ( i == field_0x414 ) {
            unk_Vector3 lift = up;
            lift *= 0.01f;
            point += lift;
            unk_Vector3 selectedScale( 0.7f, 0.7f, 0.7f );
            RwMatrixScale( &matrix, &selectedScale, rwCOMBINEPOSTCONCAT );
        } else {
            RwMatrixScale( &matrix, &scale, rwCOMBINEPOSTCONCAT );
        }
        matrix.pos.x = point.x;
        matrix.pos.y = point.y;
        matrix.pos.z = point.z;
        lbl_804780C0[i]->fn_8009CE2C( &matrix );
        RwFrameUpdateObjects( lbl_804780C0[i]->field_0x30 );
    }
}

/*
====================
CDrawingTest::fn_801796F8

Moves the crayon in hand to where the pointer's ray meets the plane of the drawing, and paints
while the button (tweaks field_0x56) is held.
NonMatching: the texture-coordinate block keeps other FPRs (retail loads 0.2666 and 0.2 before
the subtraction and adds 0.5 as the first operand); the pool order (0.2, 0.2666, 256) puts the
0.2 division first in the text. Ruled out: one-expression x/y lines, const named sizes, a
unk_Vector3 size, compound /= on local, 0.5 added in the division statements.
(Also MATCH-PENDING R18: the vector helpers are inline asm.)
====================
*/
// 0x801796F8
void CDrawingTest::fn_801796F8( unk_RwCamera* camera ) {
    if ( field_0x418 < 0 ) {
        return;
    }
    unk_Tweaks* tweaks = fn_800067B4();
    field_0x474 = tweaks->field_0x19 ? 0.5f : 1.0f;
    if ( field_0x4B4 != tweaks->field_0x56 ) {
        if ( field_0x4B4 ) {
            PostRegisteredMsg( field_0x4C4, NULL );
        } else {
            PostRegisteredMsg( field_0x4BC, NULL );
        }
        field_0x4B4 = tweaks->field_0x56;
    }

    unk_RwMatrix inverseView;
    RwMatrixInvert( &inverseView, RwCameraGetViewMatrix( camera ) );
    unk_RwV3d screen;
    screen.x = tweaks->field_0x28 / 640.0f;
    screen.y = tweaks->field_0x2C / GetDisplayHeight();
    screen.z = 1.0f;
    unk_Vector3 point;
    RwV3dTransformPoint( &point, &screen, &inverseView );

    unk_Vector3 eye = RwFrameGetLTM( RwCameraGetFrame( camera ) )->pos;
    unk_Vector3 ray = eye - point;
    V3dNormalize( &ray );
    unk_Vector3 offset = eye - *fn_8009CD08();
    float distance = V3dDot( &offset, fn_8009CD4C() );
    ray *= distance / -V3dDot( &ray, fn_8009CD4C() );
    point = eye + ray;

    unk_Vector3 scale( 0.4f, 0.4f, 0.4f );
    unk_RwMatrix matrix;
    lbl_804780C0[field_0x418]->fn_8009CDA4( &matrix );
    unk_Vector3 axis = *fn_8009CD20();
    V3dNormalize( &axis );
    RwMatrixSetIdentity( &matrix );
    RwMatrixScale( &matrix, &scale, rwCOMBINEPOSTCONCAT );
    RwMatrixRotate( &matrix, &axis, 50.0f, rwCOMBINEPOSTCONCAT );
    matrix.pos.x = point.x;
    matrix.pos.y = point.y;
    matrix.pos.z = point.z;
    lbl_804780C0[field_0x418]->fn_8009CE2C( &matrix );

    unk_RwMatrix inverse;
    RwMatrixInvert( &inverse, RwFrameGetMatrix( field_0x30 ) );
    unk_Vector3 local = point;
    RwV3dTransformPoint( &local, &local, &inverse );
    // the drawing is 0.26666668 by 0.2 and its centre 0.05 along x from the frame's origin
    local.x -= 0.05f;
    float v = local.z / 0.2f;
    float u = local.x / 0.26666668f;
    u += 0.5f;
    v += 0.5f;
    int x = 256.0f * u;
    int y = 256.0f * v;
    bool painting = false;
    if ( x >= 0 && x < 256u && y >= 0 && y < 256u ) {
        field_0x424.x = field_0x41C.x;
        field_0x424.y = field_0x41C.y;
        field_0x41C.x = x;
        field_0x41C.y = y;
        if ( field_0x4B4 ) {
            if ( field_0x42C++ == 0 ) {
                field_0x424.x = field_0x41C.x;
                field_0x424.y = field_0x41C.y;
            }
            fn_80179CC0();
            painting = true;
        } else {
            field_0x42C = 0;
        }
    }
    if ( field_0x4B8 != painting ) {
        if ( !field_0x4B8 ) {
            lbl_80529D20->fn_800ED314( 0x18, 0 );
            field_0x4B8 = true;
        } else if ( field_0x4B8 ) {     // quirk: tests the flag again (retail's beq cr1)
            lbl_80529D20->fn_800ED3C8( 0x18, 0 );
            field_0x4B8 = false;
        }
    }
}

/*
====================
CDrawingTest::PaintDot

Paints a disc of the crayon's colour into the drawing. Name invented.
====================
*/
inline void CDrawingTest::PaintDot( u8* pixels, const unk_RwV2d& centre, float radius ) {
    int r = radius;
    int cx = centre.x;
    int cy = centre.y;
    for ( int i = -r; i != r; i++ ) {
        int x = cx + i;
        u8* column = &pixels[x * 4];
        for ( int j = -r; j != r; j++ ) {
            float distance = sqrt( (float)( i * i + j * j ) );
            if ( distance <= radius ) {
                field_0x400 = true;
                if ( x >= 0 && x < 256u && cy + j >= 0 && cy + j < 256u ) {
                    int y = cy + j;
                    column[field_0x410 * y + 0] = field_0x46C.red;
                    column[field_0x410 * y + 1] = field_0x46C.green;
                    column[field_0x410 * y + 2] = field_0x46C.blue;
                }
            }
        }
    }
}

/*
====================
CDrawingTest::fn_80179CC0

Paints a stroke: dots from the previous pointer position (field_0x424) to the current one
(field_0x41C), one pixel apart, then a dot at the current one.
NonMatching: register allocation (retail gives this r31 and the pixels r24; the second PaintDot
expansion is scheduled differently).
====================
*/
// 0x80179CC0
void CDrawingTest::fn_80179CC0() {
    unk_RwV2d delta;
    delta.x = field_0x41C.x - field_0x424.x;
    delta.y = field_0x41C.y - field_0x424.y;
    u8* pixels = field_0x3FC->cpPixels;
    float radius = field_0x470 * field_0x474;
    if ( RwV2dLength( &delta ) > 1.0f ) {
        int steps = RwV2dLength( &delta );
        unk_RwV2d point = field_0x424;
        float scale = 1.0f / steps;
        RwV2dScale( &delta, &delta, scale );
        for ( int i = 0; i < steps; i++ ) {
            PaintDot( pixels, point, radius );
            RwV2dAdd( &point, &point, &delta );
        }
    }
    PaintDot( pixels, field_0x41C, radius );
}

/*
====================
CCrayon::~CCrayon
====================
*/
// 0x8017A02C
CCrayon::~CCrayon() {}

/*
====================
CCrayon::fn_801A73A8
====================
*/
// 0x8017A084
void CCrayon::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBasicAnimatedObject::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_8052A344 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetIndex:
            index = it->GetU32();
            break;
        case CMD_SetColour: {
            unk_RGBA value = it->GetRGBA();
            colour.red = value.red;
            colour.green = value.green;
            colour.blue = value.blue;
            colour.alpha = 255;
            break;
        }
        case CMD_SetSize:
            size = it->GetFloat();
            break;
        }
        ++it;
    }
}

/*
====================
CCrayon::fn_8009CB1C
====================
*/
// 0x8017A160
void CCrayon::fn_8009CB1C( unk_Msg& msg ) {
    CBasicAnimatedObject::fn_8009CB1C( msg );
}

/*
====================
CCrayon::fn_801A7A88

Files the crayon in the crayon table.
====================
*/
// 0x8017A164
void CCrayon::fn_801A7A88() {
    CBasicAnimatedObject::fn_801A7A88();
    lbl_804780C0[index] = this;
}
