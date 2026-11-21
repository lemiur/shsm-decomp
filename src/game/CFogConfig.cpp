// Unit name: the class name string "CFogConfig" (in the unit's static initialiser, fn_80092990),
// which the class registry hashes; the file name is the class name.

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <climax/Heap.h>
#include <game/CFogConfig.h>
#include <climax/EventHandler.h>
#include <climax/AttributeHandler.h>
#include <climax/Image.h>
#include <game/Messages.h>
#include <game/unk_80491B38.h>

#include <climax/RenderState.h>

CFogConfig* lbl_80529738;   // head of the registry chain of CFogConfig objects
u32 lbl_8052973C = fn_801A7A84( "CFogConfig" );
unk_801A8F6C lbl_80529740( "CFogConfig", CFogConfig::fn_800929F0, sizeof( CFogConfig ) );
unk_FlaggedNode* lbl_80529998;  // head of the flagged list, walked by fn_80092600

/*
====================
CFogConfig::CFogConfig
====================
*/
// 0x80091BFC
CFogConfig::CFogConfig( const unk_AttributePacket& packet ) {
    particles = NULL;
    particleCount = 0x5DC;
    field_0x48 = 13.0f;
    field_0x4C = 3.0f;
    field_0x54 = 0.3f;
    field_0xA4 = 0;
    field_0x50 = 25.0f;
    field_0x78 = 0.0f;
    field_0x7C = 0.0f;
    field_0x80 = 1.0f;
    field_0x84 = 0.0f;
    field_0x88 = 0.0f;
    field_0x8C = 0.65f;
    field_0x90 = 1.0f;
    field_0x94 = 0.7f;
    field_0x98 = 0.23f;
    field_0x9C = 0.37f;
    field_0x58 = 0.75f;
    field_0x5C = NULL;
    field_0x64 = 4.0f;
    field_0x68 = 2.2f;
    field_0x6C = 20.0f;
    field_0x70 = 0.2f;
    field_0x74 = 0.5f;
    field_0x60 = 0;
    texture = NULL;
    CEventHandler::fn_801A9DE8( lbl_8052A788 );
    nextFogConfig = lbl_80529738;
    lbl_80529738 = this;
}

/*
====================
CFogConfig::~CFogConfig
====================
*/
// 0x80091D40
// Wall (register/schedule, 98.8%): the list walk and calls match; retail keeps the zero head in r0 and we use r6.
CFogConfig::~CFogConfig() {
    CFogConfig* kept = NULL;
    CFogConfig* node = lbl_80529738;
    lbl_80529738 = NULL;
    while ( node != NULL ) {
        CFogConfig* cur = node;
        node = cur->nextFogConfig;
        if ( cur != this ) {
            cur->nextFogConfig = kept;
            kept = cur;
            lbl_80529738 = cur;
        }
    }
    CEventHandler::fn_801A9E00( lbl_8052A788 );
    if ( particles != NULL ) {
        delete[] particles;
        particles = NULL;
    }
    if ( field_0x5C != NULL ) {
        delete[] field_0x5C;
        field_0x5C = NULL;
    }
}

/*
====================
CFogConfig::fn_801A7A88
====================
*/
// 0x80092034
void CFogConfig::fn_801A7A88() {
    if ( textureName != NULL ) {
        texture = FindRaster( textureName, 1 );
    }
}

/*
====================
CFogConfig::fn_8009CB1C
====================
*/
// 0x80091E28
void CFogConfig::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788 ) {
        fn_80092074( &lbl_80491B38 );
    }
}

/*
====================
CFogConfig::fn_801A73A8

Reads the fog's attribute commands. Names invented.
====================
*/
// 0x80091E80
void CFogConfig::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    for ( unk_AttributeCommandIterator it( packet, lbl_8052973C ); !it.IsFinished(); ++it ) {
        switch ( it->id ) {
            case CMD_SetTextureName:
                textureName = it->GetString();
                break;
            case CMD_SetParticleCount:
                particleCount = it->GetU32();
                if ( particles != NULL ) {
                    delete[] particles;
                    particles = NULL;
                }
                break;
            case CMD_SetField48:
                field_0x48 = it->GetFloat();
                break;
            case CMD_SetField50:
                field_0x50 = it->GetFloat();
                break;
            case CMD_SetField4C:
                field_0x4C = it->GetFloat();
                break;
            case CMD_SetField54:
                field_0x54 = it->GetFloat();
                break;
            case CMD_SetField58:
                field_0x58 = it->GetFloat();
                break;
            case CMD_SetFieldA4:
                field_0xA4 = it->GetU32();
                break;
            case CMD_SetField94:
                field_0x94 = it->GetFloat();
                break;
            case CMD_SetField9C:
                field_0x9C = it->GetFloat();
                break;
            case CMD_SetField8C:
                field_0x8C = it->GetFloat();
                break;
            case CMD_SetField90:
                field_0x90 = it->GetFloat();
                break;
            case CMD_SetField88:
                field_0x88 = it->GetFloat();
                break;
            case CMD_SetField60:
                field_0x60 = it->GetU32();
                if ( field_0x5C != NULL ) {
                    delete[] field_0x5C;
                    field_0x5C = NULL;
                }
                break;
            case CMD_SetField64:
                field_0x64 = it->GetFloat();
                break;
            case CMD_SetField6C:
                field_0x6C = it->GetFloat();
                break;
            case CMD_SetField68:
                field_0x68 = it->GetFloat();
                break;
            case CMD_SetField70:
                field_0x70 = it->GetFloat();
                break;
            case CMD_SetField74:
                field_0x74 = it->GetFloat();
                break;
        }
    }
}

/*
====================
CFogConfig::fn_80092074

Fills the particle arrays on first use, then passes both arrays to fn_80092348 and reads the
player's height. Name invented.
====================
*/
// 0x80092074
void CFogConfig::fn_80092074( unk_80491B38* state ) {
    if ( particles == NULL && state->field_0x14 != NULL ) {
        particles = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_FogParticle[particleCount];
        memset( particles, 0, particleCount * sizeof( unk_FogParticle ) );
        unk_FogParticle* p = particles;
        for ( s32 i = 0; i < particleCount; i++ ) {
            p->field_0x0 = field_0x48 * ( 2.0f * ( (float)rand() / 32767.0f ) - 1.0f );
            p->field_0x4 = field_0x4C * ( (float)rand() / 32767.0f );
            p->field_0x8 = field_0x48 * ( 2.0f * ( (float)rand() / 32767.0f ) - 1.0f );
            p++;
        }
        field_0xA0 = state->field_0x14->field_0x30->modelling.pos.y;
    }
    if ( field_0x60 > 0 && field_0x5C == NULL && state->field_0x14 != NULL ) {
        field_0x5C = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_FogParticle[field_0x60];
        memset( field_0x5C, 0, field_0x60 * sizeof( unk_FogParticle ) );
        unk_FogParticle* p = field_0x5C;
        for ( s32 i = 0; i < field_0x60; i++ ) {
            p->field_0x0 = field_0x64 * ( 2.0f * ( (float)rand() / 32767.0f ) - 1.0f );
            p->field_0x4 = field_0x4C * ( (float)rand() / 32767.0f );
            p->field_0x8 = field_0x64 * ( 2.0f * ( (float)rand() / 32767.0f ) - 1.0f );
            p++;
        }
    }
    if ( state->field_0x14 != NULL ) {
        fn_80092348( state, particles, particleCount, field_0x48, field_0x4C );
        fn_80092348( state, field_0x5C, field_0x60, field_0x64, field_0x68 );
        field_0xA0 = state->field_0x14->field_0x30->modelling.pos.y;
    }
}

/*
====================
CFogConfig::fn_80092348

Drifts one particle array with the wind (field_0x78..0x84), follows the player's height, and wraps
each particle back into the box of half-width spread around the player. Name invented.
====================
*/
// Wall (dead local, 93.7%): retail keeps three zero words at frame+0x2c that no code reads, which
// shifts its frame and register allocation; not reproduced without a dummy local.
// 0x80092348
void CFogConfig::fn_80092348( unk_80491B38* state, unk_FogParticle* array, s32 count, float spread, float minimum ) {
    if ( array == NULL ) {
        return;
    }
    unk_RwV3d center = state->field_0x14->field_0x30->modelling.pos;
    unk_RwV3d lo = { center.x - spread, center.y, center.z - spread };
    unk_RwV3d hi = { center.x + spread, center.y, center.z + spread };
    float span = 2.0f * spread;

    field_0x78 += field_0x8C * ( state->field_0x8 * (float)sin( field_0x80 ) );
    field_0x7C += field_0x8C * ( state->field_0x8 * (float)cos( field_0x80 ) );
    field_0x80 += state->field_0x8 * field_0x98;
    field_0x84 -= state->field_0x8 * field_0x9C;

    float driftX = field_0x94 * ( state->field_0x8 * (float)sin( field_0x84 ) );
    float rise = field_0x88 * state->field_0x8;
    float driftZ = field_0x94 * ( state->field_0x8 * (float)cos( field_0x84 ) );

    float dy = 0.0f;
    if ( field_0xA4 != 0 ) {
        float y = state->field_0x14->field_0x30->modelling.pos.y;
        field_0x30->modelling.pos.y = y;
        dy = y - field_0xA0;
    }
    float yStep = dy + rise;

    unk_FogParticle* p = array;
    for ( s32 i = 0; i < count; i++ ) {
        p->field_0x4 += yStep;
        p->field_0x0 += driftX;
        p->field_0x8 += driftZ;
        if ( p->field_0x0 < lo.x ) {
            p->field_0x0 += span;
        }
        if ( p->field_0x8 < lo.z ) {
            p->field_0x8 += span;
        }
        if ( p->field_0x0 > hi.x ) {
            p->field_0x0 -= span;
        }
        if ( p->field_0x8 > hi.z ) {
            p->field_0x8 -= span;
        }
        if ( p->field_0x4 < 0.0f ) {
            p->field_0x4 += minimum;
        }
        if ( p->field_0x4 > minimum ) {
            p->field_0x4 -= minimum;
        }
        p++;
    }
}

/*
====================
CFogConfig::fn_80092600

Draws the fog marker: a triangle fan of three immediate-mode vertices, submitted only when this fog
has its second particle array and the flagged list holds a node. The vertices sit at the origin.
Names invented.
====================
*/
// Wall (dead locals, not reproduced): retail also computes the camera frame's basis vectors and the
// player position into stack locals that nothing reads; the body keeps the observable behaviour only.
// 0x80092600
void CFogConfig::fn_80092600( unk_80491B38* state ) {
    if ( field_0x5C == NULL ) {
        return;
    }
    unk_FlaggedNode* node = lbl_80529998;
    while ( node != NULL && node->flag == 0 ) {
        node = node->nextNode;
    }
    if ( node == NULL ) {
        return;
    }

    unk_RwIm3DVertex vertices[3];
    for ( s32 i = 0; i < 3; i++ ) {
        vertices[i].objVertex.x = 0.0f;
        vertices[i].objVertex.y = 0.0f;
        vertices[i].objVertex.z = 0.0f;
    }
    if ( RwIm3DTransform( vertices, 3, NULL, IM3D_VERTEXXYZ ) != NULL ) {
        RwIm3DRenderPrimitive( IM3D_PRIMTYPE_TRIFAN );
        RwIm3DEnd();
    }
}

/*
====================
fn_800927BC

Runs the fog pass: sets the depth and fog blend states once, then draws the fog marker of every
fog config that has its second particle array, with that fog's texture. A free function under its
plain symbol (extern "C", as fn_80008828 in unk_80008828.cpp); the evidence names no owner class.
Names invented.
====================
*/
// 0x800927BC
extern "C" void fn_800927BC( unk_80491B38* state ) {
    if ( lbl_80529738 == NULL ) {
        return;
    }
    SetDepthMode( DEPTHMODE_TESTONLY );
    SetBlendMode( BLENDMODE_FOG );
    for ( CFogConfig* fog = lbl_80529738; fog != NULL; fog = fog->nextFogConfig ) {
        if ( fog->field_0x5C != NULL ) {
            RwRenderStateSet( rwRENDERSTATETEXTURERASTER, ( void* )fog->texture );
            fog->fn_80092600( state );
        }
    }
}
