#ifndef GAME_CSPLASHSCREENTRIGGER_H
#define GAME_CSPLASHSCREENTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/Messages.h>
#include <rwsdk/badevice.h>
#include <rwsdk/baimras.h>

/* Sets vertex to a white vertex at ( x, y, z ) with texture coordinates ( u, v ) and the given alpha. Name invented. */
inline void SetVertex( RwIm2DVertex* vertex, float x, float y, float z, float u, float v, u8 alpha ) {
    RwIm2DVertexSetScreenX( vertex, x );
    RwIm2DVertexSetScreenY( vertex, y );
    RwIm2DVertexSetScreenZ( vertex, z );
    RwIm2DVertexSetU( vertex, u, 1.0f );
    RwIm2DVertexSetV( vertex, v, 1.0f );
    RwIm2DVertexSetIntRGBA( vertex, 255, 255, 255, alpha );
}

struct unk_File;        // an open file of the Climax file layer; name invented

// defined in 0x801B32C8, unit not yet reconstructed: opens the file called name
unk_File*       fn_801B32C8( const char* name, int mode, int flags );
// defined in 0x801B37E4, unit not yet reconstructed: closes a file
void            fn_801B37E4( unk_File* file );
// defined in 0x801B5EB0, unit not yet reconstructed: reads the image stored in a file
RwImage*        fn_801B5EB0( unk_File* file );

extern u32 lbl_80529BD8;            // class-name hash of "CSplashScreenTrigger"
extern unk_801A8F6C lbl_80529BDC;   // type-registry entry of CSplashScreenTrigger

/*
 * Loads an image when its message arrives, then fades it in over the whole screen: the fade level
 * rises by the frame time over the duration, and each step draws the image as a quad with that alpha.
 * vtable __vt__20CSplashScreenTrigger, size 0x58. Member names invented.
 */
class CSplashScreenTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetImageName,   // text: the file name of the image
        CMD_SetShowMsg,     // text: the name of the message that loads the image
        CMD_SetDuration     // float: seconds to fade in, limited to [0.1, 20]
    };

                    CSplashScreenTrigger();
    virtual         ~CSplashScreenTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    void            fn_800CFC40( float fade );
    bool            LoadRaster();

    static CAttributeHandler* fn_800D00AC( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CSplashScreenTrigger;
    }

    const char*     imageName;  // 0x34
    unk_801A92A8    showMsg;    // 0x38
    RwRaster*       raster;     // 0x40, the image as a texture
    float           fade;       // 0x44, 0 to 1
    float           duration;   // 0x48
};

#endif
