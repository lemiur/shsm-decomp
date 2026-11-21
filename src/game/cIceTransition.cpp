#include <game/cIceTransition.h>
#include <game/Log.h>
#include <game/unk_80491B38.h>
#include <math.h>
#include <revolution/gx/GXTexture.h>
#include <revolution/os/OSCache.h>
#include <rwsdk/driver/wii/dlraster.h>

// The cIceTransition instance that the transition reads from (set by the constructor, cleared by
// the destructor).
u32 lbl_805296A0 = fn_801A7A84( "cIceTransition" );
cIceTransition* lbl_805296B0;
s32 lbl_805296B4;

/*
====================
fn_80088030

Writes the right vector of the modelling matrix to right and adds the position offset of the
current transition to pos. Returns false when there is no transition or it has not started.
====================
*/
// 0x80088030
bool fn_80088030( unk_RwV3d* right, unk_RwV3d* pos ) {
    cIceTransition* self = lbl_805296B0;
    if ( self == NULL ) {
        return false;
    }
    if ( !self->field_0x7C && self->field_0x68 == 0.0f ) {
        return false;
    }
    const unk_RwMatrix* matrix = RwFrameGetMatrix( self->field_0x30 );
    pos->x = matrix->pos.x;
    pos->y = matrix->pos.y;
    pos->z = matrix->pos.z;
    matrix = RwFrameGetMatrix( lbl_805296B0->field_0x30 );
    right->x = matrix->right.x;
    right->y = matrix->right.y;
    right->z = matrix->right.z;
    pos->x += right->x * ( ( 1.0f - lbl_805296B0->field_0x68 ) * lbl_805296B0->field_0x70 );
    pos->y += right->y * ( ( 1.0f - lbl_805296B0->field_0x68 ) * lbl_805296B0->field_0x70 );
    pos->z += right->z * ( ( 1.0f - lbl_805296B0->field_0x68 ) * lbl_805296B0->field_0x70 );
    return true;
}

/*
====================
cIceTransition::cIceTransition

Takes nothing from the attribute packet.
====================
*/
// 0x80087EC8
cIceTransition::cIceTransition( const unk_AttributePacket& packet ) {
    fn_801A9DE8( lbl_8052A788 );
    fn_801A9DE8( lbl_8052A858 );
    field_0x80 = NULL;
    field_0x68 = 0.0f;
    field_0x6C = 1.0f;
    field_0x7C = false;
    lbl_805296B0 = this;
    field_0x7D = true;
    field_0x58[0] = (RwUInt8*)RwMalloc( 0x800, 0 );
    field_0x58[1] = (RwUInt8*)RwMalloc( 0x800, 0 );
    field_0x58[2] = (RwUInt8*)RwMalloc( 0x800, 0 );
    field_0x58[3] = (RwUInt8*)RwMalloc( 0x800, 0 );
    field_0x84 = 1.0f;
    field_0x88 = false;
    field_0x8C = 0.5f;
    field_0x90 = 3.0f;
    field_0x98 = 0;
    field_0x9C = false;
    field_0xA0 = NULL;
}

/*
====================
cIceTransition::~cIceTransition
====================
*/
// 0x80088124
cIceTransition::~cIceTransition() {
    if ( field_0x9C ) {
        fn_800885C8( false );
    }
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( lbl_8052A858 );
    fn_801A9E00( field_0x34 );
    fn_801A9E00( field_0x3C );
    fn_801A9E00( field_0x44 );
    fn_801A9E00( field_0x4C );
    if ( field_0x80 != NULL ) {
        RwTextureDestroy( field_0x80 );
    }
    RwFree( field_0x58[0] );
    RwFree( field_0x58[1] );
    RwFree( field_0x58[2] );
    RwFree( field_0x58[3] );
    fn_801A9940( field_0x34 );
    fn_801A9940( field_0x3C );
    fn_801A9940( field_0x44 );
    fn_801A9940( field_0x4C );
    if ( lbl_805296B0 == this ) {
        lbl_805296B0 = NULL;
    }
    if ( field_0xA0 != NULL ) {
        delete[] field_0xA0;
    }
}

/*
====================
cIceTransition::fn_8009CB1C

The message handler. Passes the message to the base class, then starts or stops the transition
on the messages that drive it.
====================
*/
// 0x800882CC
void cIceTransition::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == lbl_8052A788.record ) {
        fn_8008863C( lbl_80491B38.field_0x8 );
    } else if ( msg.record == lbl_8052A858.record ) {
        fn_80088748( lbl_80491B38.field_0x8 );
    } else if ( msg.record == field_0x34.record ) {
        field_0x7C = true;
        field_0x68 = 0.0f;
    } else if ( msg.record == field_0x4C.record ) {
        field_0x68 = 1.0f;
        field_0x7C = false;
        fn_800885C8( true );
    } else if ( msg.record == field_0x3C.record ) {
        field_0x88 = true;
    } else if ( msg.record == field_0x44.record ) {
        field_0x88 = false;
    }
}

/*
====================
cIceTransition::fn_801A73A8

Reads the attribute commands addressed to this class: the event names of the four table entries,
the texture name, and the timing and fade floats.
====================
*/
// 0x800883C8
void cIceTransition::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );

    unk_AttributeCommandIterator it( packet, lbl_805296A0 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetFieldMsg34:
            fn_801AA218( field_0x34, it->GetString(), NULL );
            break;
        case CMD_SetField70:
            field_0x70 = it->GetFloat();
            break;
        case CMD_SetField6C:
            field_0x6C = it->GetFloat();
            break;
        case CMD_SetField74:
            field_0x74 = it->GetFloat();
            break;
        case CMD_SetField78:
            field_0x78 = it->GetFloat();
            break;
        case CMD_SetField54:
            field_0x54 = it->GetString();
            break;
        case CMD_SetFieldMsg3C:
            fn_801AA218( field_0x3C, it->GetString(), NULL );
            break;
        case CMD_SetFieldMsg44:
            fn_801AA218( field_0x44, it->GetString(), NULL );
            break;
        case CMD_SetField8C:
            field_0x8C = it->GetFloat();
            break;
        case CMD_SetField90:
            field_0x90 = it->GetFloat();
            break;
        case CMD_SetFieldMsg4C:
            fn_801AA218( field_0x4C, it->GetString(), NULL );
            break;
        }
        ++it;
    }
}

/*
====================
cIceTransition::fn_801A7A88

Finds the texture named by field_0x54 and makes its raster the transition's table: the raster's
extension gains the alpha flag, its TLUT format becomes IA8 and its TLUT object covers the raster's
1 << depth entries. The texture's U address mode is set to clamp. Reports a texture that is not
found.
====================
*/
// 0x80088518
void cIceTransition::fn_801A7A88() {
    if ( field_0x54 != NULL ) {
        RwTexture* texture = FindTexture( field_0x54, 1 );
        field_0x80 = texture;
        if ( texture != NULL ) {
            RwRaster* raster = texture->raster;
            RwGameCubeRasterExtension* ext = RASTEREXTFROMRASTER( raster );
            ext->flags |= rwGAMECUBERASTERALPHA;
            ext->tlutFmt = GX_TL_IA8;
            GXInitTlutObj( &ext->tlutObj, ext->palette, GX_TL_IA8, (u16)( 1 << raster->depth ) );
            RwTextureSetAddressingU( field_0x80, rwTEXTUREADDRESSCLAMP );
        } else {
            fn_8002438C( "Ice Transition texture \"%s\" not loaded. Effect will not render correctly.", field_0x54 );
        }
    }
}

/*
====================
cIceTransition::fn_800885C8

Switches the table of s_IceSwitch entries on or off, writing each target its on or off value.
Does nothing when the switch is already in that state.
====================
*/
// 0x800885C8
void cIceTransition::fn_800885C8( bool enable ) {
    if ( enable == field_0x9C ) {
        return;
    }
    field_0x9C = enable;
    s_IceSwitch* entry = field_0xA0;
    if ( enable ) {
        for ( s32 i = 0; i < field_0x98; i++, entry++ ) {
            *entry->target = entry->onValue;
        }
    } else {
        for ( s32 i = 0; i < field_0x98; i++, entry++ ) {
            *entry->target = entry->offValue;
        }
    }
}

/*
====================
cIceTransition::fn_8008863C

Advances a running transition by frameTime: moves the speed field_0x84 toward 1.0, or toward its
floor while the transition runs backwards, then adds the step to the progress field_0x68. Ends the
transition at full progress and switches the table on. Does nothing when the transition is not
running.
====================
*/
// 0x8008863C
void cIceTransition::fn_8008863C( float frameTime ) {
    if ( !field_0x7C ) {
        return;
    }
    float step = frameTime * ( ( 1.0f - field_0x8C ) / field_0x90 );
    if ( field_0x88 ) {
        field_0x84 -= step;
        if ( field_0x84 < field_0x8C ) {
            field_0x84 = field_0x8C;
        }
    } else {
        field_0x84 += step;
        if ( field_0x84 > 1.0f ) {
            field_0x84 = 1.0f;
        }
    }
    field_0x7D = true;
    field_0x68 += field_0x84 * ( frameTime / field_0x6C );
    if ( field_0x68 > 1.0f ) {
        field_0x68 = 1.0f;
        field_0x7C = false;
        fn_800885C8( true );
    }
}

/*
====================
cIceTransition::fn_800886E8

Sets the texture of the running transition and loads the texture matrix of the table. Returns NULL
when there is no transition. The caller in auto_03 (fn_800BBC44) passes its own arguments through
and returns this result; the body reads none of them.
====================
*/
// 0x800886E8
cIceTransition* cIceTransition::fn_800886E8() {
    if ( lbl_805296B0 == NULL ) {
        return NULL;
    }
    RwClimaxSetTexture( lbl_805296B0->field_0x80, 3 );
    GXLoadTexMtxImm( lbl_8046A568, GX_TEXMTX3, GX_MTX2x4 );
    return this;
}

/*
====================
cIceTransition::fn_80088748

Sets the transition's matrix and table for the frame: the frame's inverse matrix, offset and scaled
by the transition's size, is loaded as the GX texture matrix. The table index advances one frame on
each call, and the texture of the running transition takes that frame's palette and fade level.
The argument is unused by the body.
Open: register/schedule wall (6 rows): the table counter store and the texture-field load are scheduled in a different order, with r3/r4 swapped; the float terms match.
====================
*/
// 0x80088748
void cIceTransition::fn_80088748( float frameTime ) {
    float x = ( 1.0f - field_0x68 ) * field_0x70;
    x /= 0.4296875f * field_0x74;
    float base = floor( x );
    float tx = 0.4296875f * base * field_0x74;
    float ty = 0.25f * base * field_0x78;
    field_0x94 = (s32)( 250.0f * ( 1.0f - ( x - base ) ) );
    unk_RwMatrix matrix;
    RwMatrixInvert( &matrix, RwFrameGetMatrix( field_0x30 ) );
    unk_RwV3d offset;
    offset.x = -tx;
    offset.y = ty;
    offset.z = 0.0f;
    unk_RwV3d scale;
    scale.x = 1.0f / field_0x74;
    scale.y = 1.0f / field_0x78;
    scale.z = 0.0f;
    RwMatrixTranslate( &matrix, &offset, rwCOMBINEPOSTCONCAT );
    RwMatrixScale( &matrix, &scale, rwCOMBINEPOSTCONCAT );
    RwClimaxRVLConvertMatrix( &matrix, lbl_8046A568 );
    s32 index = ( lbl_805296B4 + 1 ) & 3;
    lbl_805296B4 = index;
    u32 frames[4] = { 8, 9, 10, 11 };
    if ( field_0x80 != NULL ) {
        RwGameCubeRasterExtension* ext = RASTEREXTFROMRASTER( field_0x80->raster );
        ext->palette = field_0x58[index];
        fn_802B3520( 3, frames[index] );
        fn_800888F0( field_0x80, field_0x94 );
    }
}

/*
====================
fn_800888F0

Fills the 256-entry palette of the texture's raster for the given level: entry i takes the level
less i, scaled by 30 and clamped to 0..255 in both bytes, with the last two entries fixed. Flushes
the palette to memory and makes it the raster's TLUT of 1 << depth entries.
====================
*/
// 0x800888F0
void fn_800888F0( RwTexture* texture, s32 level ) {
    RwRaster* raster = texture->raster;
    RwGameCubeRasterExtension* ext = RASTEREXTFROMRASTER( raster );
    if ( ext->palette != NULL ) {
        u16* entries = (u16*)ext->palette;
        entries[254] = 0xFFFF;
        entries[253] = 0;
        for ( s32 i = 0; i < 251; i++ ) {
            s32 value = ( level - i ) * 30;
            if ( value < 0 ) {
                value = 0;
            }
            if ( value > 0xFF ) {
                value = 0xFF;
            }
            entries[i] = value | ( value << 8 );
        }
        DCFlushRange( entries, 0x200 );
    }
    ext->flags |= rwGAMECUBERASTERALPHA;
    ext->tlutFmt = GX_TL_IA8;
    GXInitTlutObj( &ext->tlutObj, ext->palette, GX_TL_IA8, (u16)( 1 << raster->depth ) );
}


/*
====================
cIceTransitionTrigger::~cIceTransitionTrigger

Detaches the trigger's event from the CEventHandler it was registered with and releases its
message id. The base and the member are destroyed by the compiler after the body.
====================
*/
// 0x80088AD4
cIceTransitionTrigger::~cIceTransitionTrigger() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9940( field_0x34 );
}

/*
====================
cIceTransitionTrigger::fn_80088EA0

Factory of the trigger class: allocates the behaviour on the game heap and constructs it in place.
The packet is not read.
====================
*/
// 0x80088EA0
CAttributeHandler* cIceTransitionTrigger::fn_80088EA0( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) cIceTransitionTrigger();
}
