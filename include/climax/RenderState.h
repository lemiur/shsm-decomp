#ifndef CLIMAX_RENDERSTATE_H
#define CLIMAX_RENDERSTATE_H

// unit name unconfirmed: header name invented. The renderer's state caches and the inline helpers
// that set a render state only when it differs from the one last set; the owning unit is not yet
// reconstructed (the caches sit in .sdata 0x80528868-0x80528874 and .sbss 0x8052A6E0-0x8052A6F0).

#include <rwsdk/badevice.h>
#include <revolution/gx/GXPixel.h>

/* The renderer's blend modes (lbl_80528868); the values of the modes used so far. Names invented. */
enum {
    BLENDMODE_OPAQUE    = 1,    // source only
    BLENDMODE_ALPHA     = 2,    // source alpha over inverse source alpha
    BLENDMODE_FOG       = 7,    // source alpha over one, the fog pass
    BLENDMODE_SUBTRACT  = 10,   // source alpha subtracted from the destination
    BLENDMODE_MULTIPLY  = 11    // source times destination
};

/* The depth modes (lbl_8052886C). Names invented. */
enum {
    DEPTHMODE_OFF,              // no depth test, no depth write
    DEPTHMODE_ON,               // depth test and write
    DEPTHMODE_TESTONLY          // depth test without write
};

/* The fog colours the blend modes ask for (lbl_8052A6E8). Names invented. */
enum {
    FOGCOLOUR_SCENE,            // the scene's fog colour, lbl_8052A6E0
    FOGCOLOUR_BLACK,
    FOGCOLOUR_WHITE,
    FOGCOLOUR_GREY
};

/* Render state caches. Names of the roles invented. */
extern u32  lbl_80528868;       // the blend mode last set
extern u32  lbl_8052886C;       // the depth mode last set: 0 off, 1 test and write, 2 test only
extern u32  lbl_80528870;       // the cull mode last set; 0 is none
extern u32  lbl_8052A6E0;       // fog colour
extern u8   lbl_8052A6E4;       // fog is on
extern u32  lbl_8052A6E8;       // the fog colour the blend mode last set asks for (FOGCOLOUR_*)
extern u32  lbl_8052A6EC;       // bit 0: front and back faces are swapped

/* Sets the cull mode if it differs from the one last set. Name invented. */
inline void SetCullMode( int mode ) {
    if ( mode != lbl_80528870 ) {
        switch ( mode ) {
        case 0:
            RwRenderStateSet( rwRENDERSTATECULLMODE, ( void* )rwCULLMODECULLNONE );
            break;
        case 1:
            if ( lbl_8052A6EC & 1 ) {
                RwRenderStateSet( rwRENDERSTATECULLMODE, ( void* )rwCULLMODECULLFRONT );
            } else {
                RwRenderStateSet( rwRENDERSTATECULLMODE, ( void* )rwCULLMODECULLBACK );
            }
            break;
        case 2:
            if ( lbl_8052A6EC & 1 ) {
                RwRenderStateSet( rwRENDERSTATECULLMODE, ( void* )rwCULLMODECULLBACK );
            } else {
                RwRenderStateSet( rwRENDERSTATECULLMODE, ( void* )rwCULLMODECULLFRONT );
            }
            break;
        }
        lbl_80528870 = mode;
    }
}

/* Sets the depth mode (depth test and write) if it differs from the one last set. Name invented. */
inline void SetDepthMode( u32 mode ) {
    if ( mode != lbl_8052886C ) {
        switch ( mode ) {
        case 0:
            RwRenderStateSet( rwRENDERSTATEZWRITEENABLE, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEZTESTENABLE, ( void* )0 );
            break;
        case 1:
            RwRenderStateSet( rwRENDERSTATEZWRITEENABLE, ( void* )1 );
            RwRenderStateSet( rwRENDERSTATEZTESTENABLE, ( void* )1 );
            break;
        case 2:
            RwRenderStateSet( rwRENDERSTATEZWRITEENABLE, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEZTESTENABLE, ( void* )1 );
            break;
        }
        lbl_8052886C = mode;
    }
}

/*
 * Sets the blend mode if it differs from the one last set, then the fog colour the mode asks for.
 * Leaving the additive modes (0, 9, 10) first resets the GX blend. Only the modes used by the units
 * reconstructed so far are written out; the renderer has thirteen (0 to 12). Name invented.
 */
inline void SetBlendMode( u32 mode ) {
    u32 state = lbl_80528868;
    if ( mode != state ) {
        if ( state >= 9 && state <= 10 || state == 0 ) {
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDONE );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDONE );
            GXSetBlendMode( GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP );
        }
        switch ( mode ) {
        case BLENDMODE_OPAQUE:
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDONE );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDZERO );
            RwRenderStateSet( rwRENDERSTATEVERTEXALPHAENABLE, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, ( void* )rwALPHATESTFUNCTIONALWAYS );
            lbl_8052A6E8 = FOGCOLOUR_SCENE;
            break;
        case BLENDMODE_ALPHA:
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDSRCALPHA );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDINVSRCALPHA );
            RwRenderStateSet( rwRENDERSTATEVERTEXALPHAENABLE, ( void* )1 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTIONREF, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, ( void* )rwALPHATESTFUNCTIONGREATER );
            lbl_8052A6E8 = FOGCOLOUR_SCENE;
            break;
        case BLENDMODE_FOG:
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDSRCALPHA );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDONE );
            RwRenderStateSet( rwRENDERSTATEVERTEXALPHAENABLE, ( void* )1 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTIONREF, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, ( void* )rwALPHATESTFUNCTIONGREATER );
            lbl_8052A6E8 = FOGCOLOUR_BLACK;
            break;
        case BLENDMODE_SUBTRACT:
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTIONREF, ( void* )0 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, ( void* )rwALPHATESTFUNCTIONGREATER );
            RwRenderStateSet( rwRENDERSTATEVERTEXALPHAENABLE, ( void* )1 );
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDSRCALPHA );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDONE );
            GXSetBlendMode( GX_BM_SUBTRACT, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_NOOP );
            lbl_8052A6E8 = FOGCOLOUR_BLACK;
            break;
        case BLENDMODE_MULTIPLY:
            RwRenderStateSet( rwRENDERSTATESRCBLEND, ( void* )rwBLENDDESTCOLOR );
            RwRenderStateSet( rwRENDERSTATEDESTBLEND, ( void* )rwBLENDZERO );
            RwRenderStateSet( rwRENDERSTATEVERTEXALPHAENABLE, ( void* )1 );
            RwRenderStateSet( rwRENDERSTATEALPHATESTFUNCTION, ( void* )rwALPHATESTFUNCTIONALWAYS );
            lbl_8052A6E8 = FOGCOLOUR_WHITE;
            break;
        }
        lbl_80528868 = mode;
        if ( lbl_8052A6E4 ) {
            switch ( lbl_8052A6E8 ) {
            case FOGCOLOUR_SCENE:
                RwRenderStateSet( rwRENDERSTATEFOGCOLOR, ( void* )lbl_8052A6E0 );
                break;
            case FOGCOLOUR_BLACK:
                RwRenderStateSet( rwRENDERSTATEFOGCOLOR, ( void* )0 );
                break;
            case FOGCOLOUR_WHITE:
                RwRenderStateSet( rwRENDERSTATEFOGCOLOR, ( void* )0xFFFFFFFF );
                break;
            case FOGCOLOUR_GREY:
                RwRenderStateSet( rwRENDERSTATEFOGCOLOR, ( void* )0x80808080 );
                break;
            }
        }
    }
}

#endif
