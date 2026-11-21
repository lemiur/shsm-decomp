// Unit name: class name from the registration string "cFlexFilter" (lbl_803E7E3C).

#include <math.h>
#include <game/cFlexFilter.h>
#include <game/unk_80491B38.h>

void fn_801E7A18( int value );
void fn_801E79BC( void* object );

/*
====================
cFlexFilter::cFlexFilter
====================
*/
// 0x80085250
cFlexFilter::cFlexFilter( const unk_AttributePacket& packet ) {
    field_0x3C = 0;
    field_0x40 = 1.0f;
    field_0x44 = 1.0f;
    field_0x4C = 1.0f;
    field_0x50 = 0.0f;
    field_0x54 = 1.0f;
    field_0x5C = 1.0f;
    field_0x60 = 1.0f;
    field_0x64 = 1.0f;
    field_0x6C = 0.3f;
    field_0x71 = false;
    field_0x58 = 1.0f;
    field_0x48 = 0.0f;
    field_0x74 = 1;
    field_0x78 = 1.0f;
    field_0x7C = 1;
    field_0x80 = 1.0f;
    field_0x84.fn_801A92FC( NULL );
    field_0x8C.fn_801A92FC( NULL );
    fn_801E7A18( NULL );
    fn_801A9DE8( lbl_8052A788, NULL, 0x8000 );
}

/*
====================
cFlexFilter::~cFlexFilter
====================
*/
// 0x80085348
cFlexFilter::~cFlexFilter() {
    fn_801A9E00( lbl_8052A788 );
    fn_801A9E00( field_0x84 );
    fn_801A9940( field_0x84 );
    fn_801A9E00( field_0x8C );
    fn_801A9940( field_0x8C );
    fn_801E7A18( NULL );
    fn_801E79BC( NULL );
}

/*
====================
cFlexFilter::fn_8009CB1C
====================
*/
// 0x80085424
void cFlexFilter::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    if ( msg.record == field_0x84.record ) {
        switch ( field_0x74 ) {
        case 0:
            field_0x3C = 3;
            break;
        case 1:
            field_0x3C = 4;
            break;
        case 2:
            field_0x3C = 5;
            break;
        }
    }
    if ( msg.record == field_0x8C.record ) {
        switch ( field_0x7C ) {
        case 0:
            field_0x3C = 2;
            break;
        case 1:
            field_0x3C = 6;
            break;
        case 2:
            field_0x3C = 7;
            break;
        }
    }
    if ( msg.record == lbl_8052A788.record ) {
        fn_80085760( GetPlayerRecord().field_0x8 );
    }
}

// Compiler-generated, no source of their own: fn_80085C04 (0x80085C04).
u32 lbl_80529648 = fn_801A7A84( "cFlexFilter" );
unk_801A8F6C lbl_8052964C( "cFlexFilter", cFlexFilter::fn_80085C64, sizeof( cFlexFilter ) );

// 256 RGBA entries of the colour ramp that fn_80085760 writes each frame.
u8 lbl_8046A080[0x400];

/*
====================
cFlexFilter::fn_801A73A8
====================
*/
// 0x80085520
// Register residue (98.8%): retail keeps this in r30 and the jump-table base in r31; compiled reverses them.
void cFlexFilter::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator it( packet, lbl_80529648 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case 12:
            if ( it->GetU32() != 0 ) {
                field_0x3C = 3;
            } else {
                field_0x3C = 2;
            }
            break;
        case 13:
            fn_801AA218( field_0x84, it->GetString(), NULL );
            break;
        case 14:
            fn_801AA218( field_0x8C, it->GetString(), NULL );
            break;
        case 0:
            field_0x40 = it->GetFloat();
            break;
        case 1:
            field_0x44 = it->GetFloat();
            break;
        case 2:
            field_0x4C = it->GetFloat();
            break;
        case 3:
            field_0x50 = it->GetFloat();
            break;
        case 4:
            field_0x54 = it->GetFloat();
            break;
        case 5:
            field_0x58 = it->GetFloat();
            break;
        case 6: {
            unk_RGBA color = it->GetRGBA();
            field_0x5C = ( float )color.red * ( 1.0f / 255.0f );
            field_0x60 = ( float )color.green * ( 1.0f / 255.0f );
            field_0x64 = ( float )color.blue * ( 1.0f / 255.0f );
            field_0x68 = ( float )color.alpha * ( 1.0f / 255.0f );
            break;
        }
        case 7:
            field_0x6C = it->GetFloat();
            break;
        case 8:
            field_0x74 = it->GetU32();
            break;
        case 9:
            field_0x78 = it->GetFloat();
            break;
        case 10:
            field_0x7C = it->GetU32();
            break;
        case 11:
            field_0x80 = it->GetFloat();
            break;
        case 15:
            if ( it->GetU32() != 0 ) {
                field_0x71 = true;
            } else {
                field_0x71 = false;
            }
            break;
        }
        ++it;
    }
    field_0x70 = 1;
}

/*
====================
cFlexFilter::fn_80085760
====================
*/
// Wall (control flow): retail dispatches with an 8-entry jump table whose entry 0 is the tail
// (0x80085A68); MWCC drops this empty case 0 and emits a compare chain. Register residue in the
// prologue (envelope in f25, frameTime in f26) and the inner field_0x7C switch are open too.
// 0x80085760
void cFlexFilter::fn_80085760( float frameTime ) {
    float envelope = 1.0f;
    float mix = 0.0f;

    switch ( field_0x3C ) {
    case 1:
        if ( field_0x71 ) {
            switch ( field_0x7C ) {
            case 0:
                field_0x3C = 2;
                break;
            case 1:
                field_0x3C = 6;
                break;
            case 2:
                field_0x3C = 7;
                break;
            }
        }
        break;
    case 2:
        fn_801E79BC( NULL );
        fn_801E7A18( NULL );
        field_0x3C = 0;
        field_0x48 = 0.0f;
        break;
    case 3:
        fn_801E79BC( lbl_8046A080 );
        fn_801E7A18( 1 );
        field_0x70 = 1;
        field_0x3C = 1;
        field_0x48 = 1.0f;
        break;
    case 4:
        fn_801E79BC( lbl_8046A080 );
        field_0x70 = 1;
        field_0x48 += frameTime * field_0x78;
        if ( field_0x48 > 1.0f ) {
            field_0x48 = 1.0f;
            field_0x3C = 3;
        }
        envelope = field_0x48;
        break;
    case 5: {
        fn_801E79BC( lbl_8046A080 );
        field_0x70 = 1;
        field_0x48 += frameTime * field_0x78;
        if ( field_0x48 > 1.0f ) {
            field_0x48 = 1.0f;
            field_0x3C = 3;
        }
        float phase = ( float )pow( field_0x48, field_0x6C );
        envelope = 0.5f * ( 1.0f + ( float )cos( 3.1415927f * ( 2.0f * ( phase - 0.5f ) ) ) );
        mix = 1.0f;
        if ( phase >= 0.5f ) {
            fn_801E7A18( 1 );
            mix = envelope;
            envelope = 1.0f;
        }
        break;
    }
    case 6:
        fn_801E79BC( lbl_8046A080 );
        field_0x70 = 1;
        field_0x48 -= frameTime * field_0x78;
        if ( field_0x48 <= 0.0f ) {
            field_0x48 = 0.0f;
            field_0x3C = 2;
        }
        envelope = field_0x48;
        break;
    case 7: {
        fn_801E79BC( lbl_8046A080 );
        field_0x70 = 1;
        field_0x48 -= frameTime * field_0x78;
        if ( field_0x48 <= 0.0f ) {
            field_0x48 = 0.0f;
            field_0x3C = 2;
        }
        float phase = ( float )pow( 1.0f - field_0x48, field_0x6C );
        float wave = 0.5f * ( 1.0f + ( float )cos( 3.1415927f * ( 2.0f * ( phase - 0.5f ) ) ) );
        if ( phase <= 0.5f ) {
            mix = wave;
            envelope = 1.0f;
        } else {
            fn_801E7A18( 0 );
            mix = 1.0f;
            envelope = wave;
        }
        break;
    }
    case 0:
        break;
    }

    if ( field_0x70 != 0 ) {
        const float scale = field_0x40 / 255.0f;
        const int alpha = ( int )( 255.0f * envelope );
        mix *= field_0x58;
        for ( int i = 0; i < 256; i++ ) {
            float intensity = field_0x50 + ( float )pow( ( float )i * scale, field_0x44 ) * field_0x4C;
            if ( intensity < 0.0f ) {
                intensity = 0.0f;
            }
            const float tintR = intensity * field_0x5C;
            const float tintG = intensity * field_0x60;
            const float tintB = intensity * field_0x64;
            int red = ( int )( 255.0f * ( tintR + ( 1.0f - tintR ) * mix ) );
            int green = ( int )( 255.0f * ( tintG + ( 1.0f - tintG ) * mix ) );
            int blue = ( int )( 255.0f * ( tintB + ( 1.0f - tintB ) * mix ) );
            if ( red > 255 ) {
                red = 255;
            }
            if ( green > 255 ) {
                green = 255;
            }
            if ( blue > 255 ) {
                blue = 255;
            }
            u8* entry = lbl_8046A080 + i * 4;
            entry[0] = red;
            entry[1] = green;
            entry[2] = blue;
            entry[3] = alpha;
        }
        field_0x70 = 0;
    }
}
