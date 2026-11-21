// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80009628.h>
#include <game/Display.h>
#include <game/PadRumble.h>
#include <game/unk_8000A040.h>
#include <game/unk_800E671C.h>
#include <climax/Camera.h>
#include <rwsdk/badevice.h>
#include <rwsdk/driver/wii/dltoken.h>
#include <revolution/gx/GXFrameBuffer.h>
#include <revolution/gx/GXManage.h>
#include <revolution/os/OSCache.h>

// Literal constants of this unit (.sdata2 0x8052B650..0x8052B690, compiler-generated names kept).
const float  lbl_8052B650 = 0.0f;
const float  lbl_8052B654 = 1.0f;
const float  lbl_8052B658 = 1.7777778f;
const float  lbl_8052B65C = 1.3333334f;
const float  lbl_8052B660 = 640.0f;
const double lbl_8052B668 = 320.0;
const double lbl_8052B670 = 0.5;
const float  lbl_8052B678 = 320.0f;
const float  lbl_8052B67C = 0.5f;
const double lbl_8052B680 = 4503601774854144.0;
const float  lbl_8052B688 = 65535.0f;

// Data of this unit (.sbss, .bss).
s32 lbl_80529150;
s32 lbl_80529154;
unk_DepthReadback lbl_804575A0[8];
unk_DepthQuery    lbl_804576A0[8];

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .bss 0x804575A0 size 0x100 lbl_804575A0
//   .bss 0x804576A0 size 0x320 lbl_804576A0
//   .sbss 0x80529150 size 0x4 lbl_80529150
//   .sbss 0x80529154 size 0x4 lbl_80529154
//   .sdata2 0x8052B650 size 0x4 lbl_8052B650
//   .sdata2 0x8052B654 size 0x4 lbl_8052B654
//   .sdata2 0x8052B658 size 0x4 lbl_8052B658
//   .sdata2 0x8052B65C size 0x4 lbl_8052B65C
//   .sdata2 0x8052B660 size 0x4 lbl_8052B660
//   .sdata2 0x8052B668 size 0x8 lbl_8052B668
//   .sdata2 0x8052B670 size 0x8 lbl_8052B670
//   .sdata2 0x8052B678 size 0x4 lbl_8052B678
//   .sdata2 0x8052B67C size 0x4 lbl_8052B67C
//   .sdata2 0x8052B680 size 0x8 lbl_8052B680
//   .sdata2 0x8052B688 size 0x8 lbl_8052B688

/*
====================
fn_80009628
====================
*/
// 0x80009628
// Queues one depth query for the owner: takes the screen point from the pad samples (or the
// tweak point when none is valid), copies the depth buffer under the draw-sync token, and
// records the camera's view matrix with the query.
// Stop at 93%: the wall is register/schedule and a control-flow shape (the aspect test laid out
// as beq then b), with the sample loop's induction form also differing.
extern "C" void fn_80009628(void* owner) {
    s32 cur = lbl_80529150;
    unk_RwCamera* camera = (unk_RwCamera*)RWSRCGLOBAL(curCamera);
    unk_DepthQuery* rec = &lbl_804576A0[cur];
    unk_DepthReadback* readback = &lbl_804575A0[cur];
    f32 pointX;
    f32 pointY;
    BOOL drawn;
    f32 sumX;
    f32 sumY;
    s32 count;
    s32 i;

    lbl_80529150 = (cur + 1) % 8;
    lbl_80529154 += 1;
    if (lbl_80529154 < 0) {
        lbl_80529154 = 8;
    }
    rec->frame = lbl_80529154;

    pointX = (f32)(s32)fn_800067B4()->field_0x28;
    pointY = (f32)(s32)fn_800067B4()->field_0x2C;
    drawn = fn_800067B4()->field_0x30;

    sumX = 0.0f;
    sumY = 0.0f;
    count = 0;
    for (i = 0; i < 16 && count < 8; i++) {
        if (lbl_804579C0[0].samples[i].dpd_valid_fg == 2) {
            count++;
            sumX += lbl_804579C0[0].samples[i].pos.x;
            sumY += lbl_804579C0[0].samples[i].pos.y;
        }
    }

    if (count != 0) {
        f32 meanX = sumX / (f32)count;
        f32 meanY = sumY / (f32)count;
        f32 invRange = 1.0f / lbl_8045A680.range;
        f32 aspectScale = 1.0f;

        if (!lbl_80529E44->field_0x251) {
            f32 aspect = lbl_80529E44->field_0x250 ? 1.7777778f : 1.3333334f;
            aspectScale = 1.0f / (aspect / (640.0f / (f32)GetDisplayHeight()));
            aspectScale = aspectScale * lbl_80528628;
        }

        pointX = (f32)(meanX * (320.0 * invRange)) + 320.0f;
        pointY = (f32)(meanY * (0.5 * (640.0f * aspectScale) * invRange)) +
                 0.5f * (f32)GetDisplayHeight();
        drawn = TRUE;
    }

    {
        s32 sx = (s32)pointX;
        s32 sy = (s32)pointY;
        rec->screenX = sx;
        rec->screenY = sy;
        rec->sampleIndex = (sx & 1) + ((sy & 1) << 2);
    }
    if (rec->screenX < 0) {
        rec->screenX = 0;
    }
    if (rec->screenY < 0) {
        rec->screenY = 0;
    }
    if (rec->screenX >= 640) {
        rec->screenX = 639;
    }
    if (rec->screenY >= GetDisplayHeight()) {
        rec->screenY = GetDisplayHeight() - 1;
    }

    if (drawn) {
        GXSetCopyFilter(GX_FALSE, NULL, GX_FALSE, NULL);
        GXSetTexCopySrc(rec->screenX & 0x3FE, rec->screenY & 0x3FE, 2, 2);
        GXSetTexCopyDst(4, 4, GX_TF_Z16, GX_FALSE);
        GXCopyTex(readback, GX_FALSE);
        GXSetDrawSync(_RwDlTokenCurrent);
        _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % 0xE000;
        GXSetCopyFilter(lbl_8052B2A4->aa, lbl_8052B2A4->sample_pattern, GX_TRUE, lbl_8052B2A4->vfilter);
        rec->token = _RwDlTokenCurrent;
        GXSetDrawSync(_RwDlTokenCurrent);
        rec->active = TRUE;
        _RwDlTokenCurrent = (_RwDlTokenCurrent + 1) % 0xE000;
    } else {
        rec->active = FALSE;
    }

    rec->matrix = camera->viewMatrix;
    rec->owner = owner;
    rec->frame = lbl_80529154;
    rec->nearPlane = camera->nearPlane;
    rec->farPlane = camera->farPlane;
}

/*
====================
fn_80009AB8
====================
*/
// 0x80009AB8
// Projects the depth sample of a query record back to view space: the newest record when
// useNewest is set, otherwise the first record whose readback is complete.
// Stop at 59%: the wall is register/schedule (frame 0xD0 against retail's 0xA0, and the byte
// swap of the sample is scheduled differently).
BOOL fn_80009AB8(void* owner, unk_RwV3d* out, BOOL useNewest) {
    unk_DepthQuery* query = NULL;
    unk_DepthQuery* newest = NULL;
    unk_DepthReadback* readback = NULL;
    s32 queryFrame = -1;
    s32 newestFrame = -1;
    s32 i;
    unk_DepthQuery* rec;
    u16 sample;
    unk_RwMatrix inverse;
    unk_RwV3d point;
    f32 invNear, invFar, depth, x, y;

    for (i = 0; i < 8 && i < lbl_80529154; i++) {
        rec = &lbl_804576A0[i];
        if (rec->active != 0 && rec->frame > queryFrame && rec->owner == owner) {
            if (_rwDlTokenQueryDone(rec->token)) {
                query = rec;
                readback = &lbl_804575A0[i];
                queryFrame = rec->frame;
            }
            if (rec->frame > newestFrame) {
                newestFrame = rec->frame;
                newest = rec;
            }
        }
    }

    if (query == NULL) {
        return FALSE;
    }

    DCInvalidateRange(readback, 0x20);
    sample = readback->samples[query->sampleIndex];
    sample = (u16)(((sample & 0xFF) << 8) | (sample >> 8));
    DCInvalidateRange(readback, 0x20);
    if (sample == 0xFFFF) {
        return FALSE;
    }

    invNear = lbl_8052B654 / query->nearPlane;
    invFar = lbl_8052B654 / query->farPlane;
    depth = lbl_8052B654 / (invNear + ((f32)sample / lbl_8052B688) * (invFar - invNear));

    if (useNewest) {
        query = newest;
    }
    x = ((f32)query->screenX / lbl_8052B660) * depth;
    y = ((f32)query->screenY / lbl_8052B660) * depth;

    RwMatrixInvert(&inverse, &query->matrix);
    point.x = x;
    point.y = y;
    point.z = depth;
    RwV3dTransformPoint(out, &point, &inverse);
    return TRUE;
}

/*
====================
fn_80009CCC
====================
*/
// 0x80009CCC
// WPAD allocator (WPADAlloc): the block allocator with the 0x02000000 flag word.
extern "C" void* fn_80009CCC(u32 size) {
    return fn_8018DDD8(size, 0x02000000);
}

/*
====================
fn_80009CD4
====================
*/
// 0x80009CD4
// WPAD free (WPADFree): releases the block and reports success.
extern "C" BOOL fn_80009CD4(void* block) {
    fn_8018DD78(block);
    return TRUE;
}
