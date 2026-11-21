// Unit name and boundary: Gormiti link map (RwCore.a dlsprite.obj)
// Names and layouts: Gormiti DWARF (dlsprite.c)

/*
 * dlsprite.c: drawing rasters into the current raster of the display-list driver: the render hooks
 * that draw a raster as a quad, and the clears of the current raster.
 */
#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <revolution/vi.h>
#include <rwsdk/baraster.h>
#include <rwsdk/driver/wii/dlraster.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/batypes.h>

/* The raster the draw and clear calls write into. */
RwRaster* lbl_8052B2C0;

/* Draws a raster as a quad into the current raster. */
RwBool _rwDlRasterRenderQuad(RwRaster* raster, RwRect* rect);

/* Clears a rect of the current raster to a packed color. */
RwBool fn_802B2894(RwRaster* raster, RwRect* rect, RwInt32 packedColor);

/* Draws a raster into the current raster. The driver's switch on the two rasters' types picks the
 * draw, or reports the combination as not supported. The body never reads alpha (the retail code
 * takes no branch on it), so the parameter is kept for the callers' signature only.
 * Open (99.64%): the three RWERROR strings are addressed as lbl_8044B1E8+0x50, +0x84 and +0xB8, offsets into
 * the literal pool that fn_802B1880's unit also addresses; this unit's pool starts at offset 0 (referent). */
RwBool fn_802B212C(RwRaster* raster, RwRect* rect, RwBool scaled, RwBool alpha)
{
    RwRect scissorRect;

    scissorRect.x = rect->x;
    scissorRect.y = rect->y;
    if (scaled) {
        scissorRect.w = rect->w;
        scissorRect.h = rect->h;
    } else {
        scissorRect.w = raster->width;
        scissorRect.h = raster->height;
    }

    switch (lbl_8052B2C0->cType) {
    case rwRASTERTYPENORMAL:
    case rwRASTERTYPETEXTURE:
    case rwRASTERTYPECAMERATEXTURE:
        switch (raster->cType) {
        case rwRASTERTYPENORMAL:
        case rwRASTERTYPEZBUFFER:
        case rwRASTERTYPECAMERA:
        case rwRASTERTYPETEXTURE:
        case rwRASTERTYPECAMERATEXTURE:
            RWERROR((2, "SRC, DST Raster render combination not supported"));
            return FALSE;
        }
        break;
    case rwRASTERTYPECAMERA:
        switch (raster->cType) {
        case rwRASTERTYPENORMAL:
        case rwRASTERTYPETEXTURE:
        case rwRASTERTYPECAMERATEXTURE:
            _rwDlRasterRenderQuad(raster, &scissorRect);
            break;
        case rwRASTERTYPEZBUFFER:
        case rwRASTERTYPECAMERA:
            RWERROR((2, "SRC, DST Raster render combination not supported"));
            return FALSE;
        }
        break;
    default:
        RWERROR((2, "SRC, DST Raster render combination not supported"));
        return FALSE;
    }
    return TRUE;
} // 0x802B212C

/* Draws a raster with the rect as its scissor, alpha on. */
RwBool _rwDlRasterRender(RwRaster* raster, RwRect* rect)
{
    return fn_802B212C(raster, rect, 0, 1);
} // 0x802B229C

/* Draws a raster with the rect as its scissor, alpha off. */
RwBool _rwDlRasterRenderFast(RwRaster* raster, RwRect* rect)
{
    return fn_802B212C(raster, rect, 0, 0);
} // 0x802B22A8

/* Draws a raster scaled to the rect, alpha on. */
RwBool _rwDlRasterRenderScaled(RwRaster* raster, RwRect* rect)
{
    return fn_802B212C(raster, rect, 1, 1);
} // 0x802B22B4

/* Clears a rect of the current raster. The raster is the hook's first argument and is not used:
 * the clear always covers the current raster. */
RwBool _rwDlRasterClearRect(RwRaster* raster, RwRect* rect, RwInt32 packedColor)
{
    return fn_802B2894(lbl_8052B2C0, rect, packedColor);
} // 0x802B2C5C

/* Clears the whole current raster to a packed color. The first two arguments are the hook's and
 * are not used. */
RwBool _rwDlRasterClear(RwRaster* raster, RwRect* rect, RwInt32 packedColor)
{
    RwRect clearRect;

    clearRect.x = 0;
    clearRect.y = 0;
    clearRect.w = lbl_8052B2C0->width;
    clearRect.h = lbl_8052B2C0->height;
    return fn_802B2894(lbl_8052B2C0, &clearRect, packedColor);
} // 0x802B2C64

/* Makes a raster the current raster that the draw and clear calls write into. The first argument
 * is the hook's and is not used. */
RwBool _rwDlSetRasterContext(RwRaster* device, RwRaster* rasIn)
{
    lbl_8052B2C0 = rasIn;
    return 1;
} // 0x802B2CA8

/* The render mode in use: the viewport, scissor and projection the camera clears follow it. */
GXRenderModeObj* lbl_8052B2A4;
/* The pixel format the camera clears want, and the one GX is set to. */
GXPixelFmt lbl_8052B224;
GXPixelFmt lbl_8052B228;
/* Two scissor bounds around the field offset lbl_8052B2A0, and a flag that picks them. */
s32 lbl_8052B2A0;
s32 lbl_8052B230;
s32 lbl_80528F20;

/* The projection the camera clear sets up: GXSetProjectionv's seven floats (type, then the scales). */
f32 lbl_8044B2D4[7] = { 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f };
/* The position matrix the camera clear loads. */
Mtx lbl_8044B2F0 = { { 1.0f, 0.0f, 0.0f, 0.5f }, { 0.0f, 1.0f, 0.0f, 0.5f }, { 0.0f, 0.0f, -1.0f, 0.0f } };

/* Sets up the GX state of the camera's Z clear: one pass-through TEV stage over the vertex color,
 * the pixel format of the raster's extension, the viewport, scissor and projection of the render
 * mode. A camera texture (cType 5) takes its viewport from the 640x528 frame, not the render mode. */
void _rwDlRasterCamera_ZClearRectInit(RwRaster* raster)
{
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);
    GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);

    if (raster->cType == rwRASTERTYPECAMERATEXTURE) {
        RwGameCubeRasterExtension* rasExt = RASTEREXTFROMRASTER(raster->parent);

        if (rasExt->flags & rwGAMECUBERASTERALPHA) {
            if (lbl_8052B224 != GX_PF_RGBA6_Z24) {
                GXSetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
                lbl_8052B228 = GX_PF_RGBA6_Z24;
            }
        } else if (lbl_8052B224 != GX_PF_RGB8_Z24) {
            GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
            lbl_8052B228 = GX_PF_RGB8_Z24;
        }
        GXSetViewport(0.0f, 0.0f, 640.0f, 528.0f, 0.0f, 1.0f);
        GXSetScissor(0, 0, 640, 528);
        lbl_8044B2D4[1] = 2.0f / 639.0f;
        lbl_8044B2D4[3] = -2.0f / 527.0f;
    } else {
        GXRenderModeObj* renderMode;

        if (lbl_8052B228 != lbl_8052B224) {
            GXSetPixelFmt(lbl_8052B224, GX_ZC_LINEAR);
            lbl_8052B228 = lbl_8052B224;
        }
        renderMode = lbl_8052B2A4;
        if (renderMode->field_rendering) {
            GXSetViewportJitter(0.0f, 0.0f, renderMode->fbWidth, renderMode->xfbHeight, 0.0f, 1.0f,
                                VIGetNextField() ^ 1);
        } else {
            GXSetViewport(0.0f, 0.0f, renderMode->fbWidth, renderMode->xfbHeight, 0.0f, 1.0f);
        }

        if (lbl_8052B230 == 0) {
            GXSetScissor(0, 0, lbl_8052B2A4->fbWidth, lbl_8052B2A4->xfbHeight);
        } else if (lbl_80528F20 != 0) {
            GXSetScissor(0, 0, lbl_8052B2A4->fbWidth, lbl_8052B2A0 + 2);
            GXSetScissorBoxOffset(0, 0);
        } else {
            GXSetScissor(0, lbl_8052B2A0 - 2, lbl_8052B2A4->fbWidth, lbl_8052B2A0 + 2);
            GXSetScissorBoxOffset(0, lbl_8052B2A0 - 2);
        }

        lbl_8044B2D4[1] = 2.0f / lbl_8052B2A4->fbWidth;
        lbl_8044B2D4[3] = -2.0f / lbl_8052B2A4->xfbHeight;
    }

    GXSetProjectionv(lbl_8044B2D4);
    GXLoadPosMtxImm(lbl_8044B2F0, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
} // 0x802B22C0

/* Open (0.6%): not reconstructed. Blocked: the body reads lbl_804F7540, the 0x50-byte render-state record of the
 * unsplit dlrendst unit, and its layout is not recovered yet (needs a shared type first). */
void _rwDlRasterCamera_ZClearRect(RwRaster* raster, RwRect* rect)
{
} // 0x802B25EC
