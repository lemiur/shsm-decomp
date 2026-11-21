// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009A024.h>
#include <climax/RenderState.h>
#include <revolution/mtx.h>
#include <revolution/gx/GXCull.h>
#include <revolution/gx/GXFrameBuffer.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXLighting.h>
#include <revolution/gx/GXPixel.h>
#include <revolution/gx/GXTev.h>
#include <revolution/gx/GXTexture.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/gx/GXVert.h>

// Literal constants of this unit (.sdata2 0x8052C2C8..0x8052C2D8; the names are the symbols.txt names).
const u32 lbl_8052C2C8 = 0xFFA60000; // GXColorS10 { -90, 0, -114, 135 }, first word
const u32 lbl_8052C2CC = 0xFF8E0087; // second word: b = -114, a = 135
const GXColor lbl_8052C2D0 = { 0x00, 0x00, 0xE2, 0x58 };
const GXColor lbl_8052C2D4 = { 0xB3, 0x00, 0x00, 0xB6 };
const GXColor lbl_8052C2D8 = { 0xFF, 0x00, 0xFF, 0x80 };

/*
====================
fn_8009A024

Sets the projection, viewport and TEV state of the screen-space pass for the rectangle.
Not matched: 97.8%; the rest is the stack-slot layout of the colour words and the width/height register binding.
====================
*/
// 0x8009A024
void fn_8009A024(const unk_ScreenSize* screen) {
    s32 width = screen->width;
    s32 height = screen->height;
    Mtx44 projection;
    Mtx identity;

    C_MTXOrtho(projection, 0.0f, height, 0.0f, width, 0.0f, -1.0f);
    GXSetProjection(projection, GX_ORTHOGRAPHIC);
    GXSetViewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
    GXSetScissor(0, 0, width, height);
    PSMTXIdentity(identity);
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetZMode(GX_TRUE, GX_ALWAYS, GX_FALSE);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetDispCopyGamma(GX_GM_1_0);
    GXSetNumChans(0);
    GXSetNumTexGens(2);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
    GXInvalidateTexAll();
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
    GXSetNumTevStages(4);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_C0);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
    GXSetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP2, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, GX_CC_CPREV);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, GX_CA_APREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K1);
    GXSetTevKAlphaSel(GX_TEVSTAGE1, GX_TEV_KASEL_K1_A);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);

    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_ONE, GX_CC_CPREV);
    GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
    GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP0);

    GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE3, GX_CC_APREV, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE3, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevKColorSel(GX_TEVSTAGE3, GX_TEV_KCSEL_K2);

    u32 tevColor[2] = { lbl_8052C2C8, lbl_8052C2CC };
    GXSetTevColorS10(GX_TEVREG0, *(GXColorS10*)tevColor);
    GXSetTevKColor(GX_KCOLOR0, lbl_8052C2D0);
    GXSetTevKColor(GX_KCOLOR1, lbl_8052C2D4);
    GXSetTevKColor(GX_KCOLOR2, lbl_8052C2D8);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
}

/*
====================
fn_8009A4CC

Draws one textured quad at (x, y) of size (dx, dy) with the three textures bound to TEXMAP0..2.
The half-size textures take half of width and height.
Not matched: 91.6%; the parameters bind to different callee-saved registers than retail (register/schedule wall).
====================
*/
// 0x8009A4CC
void fn_8009A4CC(f32 z, void* texture0, void* texture1, void* texture2, s32 x, s32 y, u16 width, u16 height, s32 dx, s16 dy) {
    GXTexObj obj0;
    GXTexObj obj1;
    GXTexObj obj2;
    s32 halfWidth = width >> 1;
    s32 halfHeight = height >> 1;

    GXInitTexObj(&obj0, texture0, width, height, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&obj0, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&obj0, GX_TEXMAP0);

    GXInitTexObj(&obj1, texture1, (u16)(width >> 1), (u16)(height >> 1), GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&obj1, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&obj1, GX_TEXMAP1);

    GXInitTexObj(&obj2, texture2, halfWidth, halfHeight, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&obj2, GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    GXLoadTexObj(&obj2, GX_TEXMAP2);

    GXBegin(GX_QUADS, GX_VTXFMT7, 4);

    GXWGFifo.f32 = x;
    GXWGFifo.f32 = y;
    GXWGFifo.f32 = z;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 0;

    GXWGFifo.f32 = x + dx;
    GXWGFifo.f32 = y;
    GXWGFifo.f32 = z;
    GXWGFifo.u16 = 1;
    GXWGFifo.u16 = 0;

    GXWGFifo.f32 = x + dx;
    GXWGFifo.f32 = y + dy;
    GXWGFifo.f32 = z;
    GXWGFifo.u16 = 1;
    GXWGFifo.u16 = 1;

    GXWGFifo.f32 = x;
    GXWGFifo.f32 = y + dy;
    GXWGFifo.f32 = z;
    GXWGFifo.u16 = 0;
    GXWGFifo.u16 = 1;
}
