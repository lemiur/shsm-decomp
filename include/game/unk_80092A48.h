#ifndef GAME_UNK_80092A48_H
#define GAME_UNK_80092A48_H

// unit name unconfirmed: class unk_80092A48 (named after its constructor address), name invented.
// Its constructor and destructor are free functions taking the object as `self`, named after their
// addresses, until the special names are recorded in symbols.txt.

#include <revolution/types.h>
#include <rwsdk/bacamera.h>
#include <rwsdk/baframe.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batextur.h>
#include <rwsdk/driver/wii/dlraster.h>
#include <rwsdk/world/baclump.h>
#include <rwsdk/world/baworobj.h>
#include <rwsdk/world/climaxcommon.h>

/*
 * The argument the caller passes in the constructor's third slot (the caller's own field at 0x130):
 * a frame whose modelling matrix is the parent transform of the object's atomic. Member names
 * invented.
 */
struct unk_80092A48Source {
    RwUInt8     field_0x00[ 4 ];    /* 0x00 */
    RwFrame*    frame;              /* 0x04 */
};

/*
 * The object built by fn_80092A48 (size 0x74). It owns an atomic drawn through a camera into a
 * 128 x 128 texture, and links itself into the global list lbl_80529748 and into the slot table
 * lbl_8046A6B8. Member names invented; the field notes are read from the constructor's stores.
 */
struct unk_80092A48 {
    RwV3d               up;             /* 0x00, (0, 1, 0) at construction */
    RwReal              field_0x0C;     /* 0x0C, 0.0 at construction */
    RwReal              field_0x10;     /* 0x10, 10.0 at construction */
    RwUInt8             field_0x14;     /* 0x14 */
    RwUInt8             field_0x15;     /* 0x15, 1 at construction */
    RwUInt8             field_0x16;     /* 0x16 */
    RwInt32             slot;           /* 0x18, index in lbl_8046A6B8 (8 when none was free) */
    RpAtomic*           atomic;         /* 0x1C */
    void*               field_0x20;     /* 0x20 */
    void*               field_0x24;     /* 0x24 */
    RwCamera*           camera;         /* 0x28 */
    RwFrame*            atomicFrame;    /* 0x2C */
    RwFrame*            sourceFrame;    /* 0x30, the source's frame */
    RwFrame*            cameraFrame;    /* 0x34 */
    RwMatrix*           atomicMatrix;   /* 0x38 */
    RwMatrix*           sourceMatrix;   /* 0x3C */
    RwMatrix*           cameraMatrix;   /* 0x40 */
    RwInt32             field_0x44;     /* 0x44 */
    unk_80092A48Source* source;         /* 0x48 */
    RwReal              field_0x4C;     /* 0x4C, the view window's y half size (0.5) */
    RwReal              field_0x50;     /* 0x50, the view window's x half size (0.5) */
    RwReal              field_0x54;     /* 0x54, 1.0 at construction */
    RwRaster*           raster;         /* 0x58 */
    RwTexture*          texture;        /* 0x5C */
    void*               field_0x60;     /* 0x60 */
    RwInt32             field_0x64;     /* 0x64 */
    RpWorld*            world;          /* 0x68 */
    unk_80092A48*       next;           /* 0x6C, the next live object in lbl_80529748 */
    RwInt32             field_0x70;     /* 0x70 */
};

extern "C" {

extern unk_80092A48* lbl_80529748;
extern RwRaster* lbl_8052974C;
extern unk_80092A48* lbl_8046A6B8[ 8 ];

// unit not yet reconstructed: releases the texture (three arguments)
void fn_801D797C( RwTexture* texture, RwInt32 unk04, RwInt32 unk05 );

unk_80092A48* fn_80092A48( unk_80092A48* self, int unused, unk_80092A48Source* source );
unk_80092A48* fn_80092CD8( unk_80092A48* self, int flag );
RpAtomic*     fn_80092DFC( RpAtomic* atomic );
void          fn_80092E48( unk_80092A48* self );
void fn_8009473C();
void fn_80094754();
void fn_80094D38();
void fn_80094DB0();
void fn_80094E04();
void fn_80094E50();
void fn_80094E68();
void fn_80094E6C();
void fn_80094F40();
void fn_80094FFC();

}

#endif
