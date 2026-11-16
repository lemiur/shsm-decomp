#ifndef GAME_CDRAWINGTEST_H
#define GAME_CDRAWINGTEST_H

// unit name unconfirmed: the class name is the registration string "CDrawingTest"; the file name
// is invented.

#include <climax/Atomic.h>
#include <climax/Camera.h>
#include <climax/Image.h>
#include <rwsdk/baimras.h>
#include <rwsdk/batextur.h>
#include <climax/Memory.h>
#include <game/CBasicAnimatedObject.h>
#include <game/CCrayon.h>
#include <game/Messages.h>
#include <game/unk_80039E40.h>
#include <game/unk_80491B38.h>
#include <string.h>

/*
 * One region of the drawing's outline map (one of the eight regions named in fn_801785B4:
 * "Nothing", "Wall", "Roof", "Mailbox", "MrsLevin", "MrLevin", "Car", "Door"): the pixels painted
 * into it per crayon, and the crayon that painted most of it. Name and member names invented.
 */
struct unk_PaintRegion {
            unk_PaintRegion() : crayon( 0 ) { memset( crayonPixels, 0, sizeof( crayonPixels ) ); }

    u32     crayon;             // 0x00, the crayon with the most pixels (more than 5), else 0
    u32     crayonPixels[12];   // 0x04, one count per crayon
};

/*
 * The painting test (registered as "CDrawingTest", vtable lbl_803FB9F8, size 0x4CC): the player
 * colours a 256 x 256 drawing with the crayons (CCrayon) of the scene, and the test counts the
 * colours painted into each region of the outline map. The image is drawn through an atomic
 * whose render callback is fn_80178580. CBasicAnimatedObject, then unk_80039E40 at 0x1D4.
 * fn_8000CFD0 and fn_8000CFCC override slots of the non-primary base CAttributeHandler, so MWCC
 * appends them to the vtable (0x110, 0x114). Member names invented; the field notes are read
 * from their uses.
 */
class CDrawingTest : public CBasicAnimatedObject, public unk_80039E40 {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetListenMsg0x3D4,  // text: a message it listens to
        CMD_SetSendMsg0x3DC,    // text: a message it sends
        CMD_SetListenMsg0x3E4,  // text: a message it listens to
        CMD_SetSendMsg0x3EC,    // text: a message it sends
        CMD_SetSendMsg0x3F4,    // text: a message it sends
        CMD_SetField0x430,      // float
        CMD_SetField0x434,      // text
        CMD_SetField0x438,      // integer
        CMD_SetField0x43C,      // text
        CMD_SetField0x440,      // integer
        CMD_SetField0x444,      // text
        CMD_SetField0x448,      // float
        CMD_SetField0x44C,      // text
        CMD_SetField0x450,      // integer
        CMD_SetField0x454,      // text
        CMD_SetSendMsg0x4BC,    // text: a message it sends
        CMD_SetSendMsg0x4C4     // text: a message it sends
    };

                    CDrawingTest( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );  // override
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );  // override
    virtual         ~CDrawingTest();  // override
    virtual void    fn_801A7A88();  // override
    virtual void    fn_8000CFD0();  // override
    virtual void    fn_8000CFCC();  // override

    // the render callback of field_0x498: draws the instance lbl_8052A340
    static unk_RpAtomic* fn_80178580( unk_RpAtomic* atomic );

    void            fn_801785B4();
    void            fn_80178A60();
    void            fn_80179304( unk_RwCamera* camera );
    void            fn_801796F8( unk_RwCamera* camera );
    void            fn_80179CC0();
    void            PaintDot( u8* pixels, const unk_RwV2d& centre, float radius );  // name invented

    static CAttributeHandler* fn_8017A270( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) CDrawingTest( packet );
    }

    unk_PaintRegion field_0x234[8];     // 0x234, per region of the outline map
    unk_801A92A8    field_0x3D4;        // 0x3D4, listened to
    unk_801A92A8    field_0x3DC;        // 0x3DC, sent
    unk_801A92A8    field_0x3E4;        // 0x3E4, listened to
    unk_801A92A8    field_0x3EC;        // 0x3EC, sent
    unk_801A92A8    field_0x3F4;        // 0x3F4, sent
    RwImage*        field_0x3FC;        // 0x3FC, the drawing: 256 x 256, 32 bits per pixel
    bool            field_0x400;        // 0x400, the drawing changed: the raster needs refreshing
    RwRaster*       field_0x404;        // 0x404, the drawing as a texture raster
    RwTexture*      field_0x408;        // 0x408, "PR_PSY02_HouseDrawing"
    RwTexture*      field_0x40C;        // 0x40C, "PR_PaintingOutlineIDMap", the region map
    s32             field_0x410;        // 0x410, the drawing's row stride
    int             field_0x414;        // 0x414, a crayon index (lbl_804780C0), -1 for none
    int             field_0x418;        // 0x418, -1 for none
    unk_RwV2d       field_0x41C;        // 0x41C, a position
    unk_RwV2d       field_0x424;        // 0x424, the previous field_0x41C
    u32             field_0x42C;        // 0x42C
    float           field_0x430;        // 0x430, a time limit (60), compared with field_0x458
    const char*     field_0x434;        // 0x434, a name passed to fn_8014EC98
    int             field_0x438;        // 0x438, a count (5), tested against field_0x45C
    const char*     field_0x43C;        // 0x43C, a name passed to fn_8014EC98
    int             field_0x440;        // 0x440, a count (8), tested against field_0x460
    const char*     field_0x444;        // 0x444, a name passed to fn_8014EC98
    float           field_0x448;        // 0x448, a share (50), compared with field_0x464
    const char*     field_0x44C;        // 0x44C, a name passed to fn_8014EC98
    int             field_0x450;        // 0x450, a count (50), tested against field_0x468
    const char*     field_0x454;        // 0x454, a name passed to fn_8014EC98
    float           field_0x458;        // 0x458, the time spent (frame times added up)
    int             field_0x45C;        // 0x45C, the number of crayons used (fn_801785B4)
    int             field_0x460;        // 0x460, the crayon changes, counted up in fn_8009CB1C
    float           field_0x464;        // 0x464, the share of the drawing painted, in percent (fn_801785B4)
    int             field_0x468;        // 0x468
    unk_RGBA        field_0x46C;        // 0x46C, the colour of the crayon in use
    float           field_0x470;        // 0x470, the size of the crayon in use
    float           field_0x474;        // 0x474, a scale (1 or lbl_8052DC14)
    u8              field_0x478[0x20];  // 0x478, cleared by the constructor; no other use seen
    unk_RpAtomic*   field_0x498;        // 0x498, the atomic that draws the drawing
    float           field_0x49C;        // 0x49C, from the tweaks (fn_800067B4) 0x28
    float           field_0x4A0;        // 0x4A0, from the tweaks 0x2C
    float           field_0x4A4;        // 0x4A4
    float           field_0x4A8;        // 0x4A8
    int             field_0x4AC;        // 0x4AC, counted up to 2
    int             field_0x4B0;        // 0x4B0, counted up to 2
    bool            field_0x4B4;        // 0x4B4
    bool            field_0x4B5;        // 0x4B5, tested by fn_80178A60
    bool            field_0x4B6;        // 0x4B6
    bool            field_0x4B7;        // 0x4B7
    bool            field_0x4B8;        // 0x4B8, set from fn_800ED314 / fn_800ED3C8
    bool            field_0x4B9;        // 0x4B9
    unk_801A92A8    field_0x4BC;        // 0x4BC, sent
    unk_801A92A8    field_0x4C4;        // 0x4C4, sent
};

extern u32 lbl_8052A338;                // class-name hash of "CDrawingTest"
extern unk_801A8F6C lbl_8052A33C;       // type-registry entry of CDrawingTest
extern CDrawingTest* lbl_8052A340;      // the instance, while one exists

// tables read by fn_801785B4 (roles unknown; names invented)
extern const char* lbl_80528500[2];             // { "SaveData", NULL }
extern const char* const lbl_8052DBD0;          // "PaintTest_"
extern const char* const lbl_803D9250[8];       // the region names of the outline map

#endif
