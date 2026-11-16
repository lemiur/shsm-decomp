// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_common.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_common.cpp
#include "nw4hbm/lyt/common.h"

#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/binaryFileFormat.h"
#include "nw4hbm/ut/inlines.h"

#include <string.h>

#include <revolution/gx.h>
#include <revolution/tpl.h>

namespace nw4hbm {
namespace lyt {
namespace detail {

/*
 * EqualsPaneName
 *
 * Whether two pane names are equal (a pane name is 16 characters, not
 * necessarily terminated).
 */
bool EqualsPaneName(const char* name1, const char* name2) {
    return strncmp(name1, name2, 16) == 0;
} // 0x803C1F30

/*
 * EqualsMaterialName
 *
 * Whether two material names are equal (a material name is 20 characters, not
 * necessarily terminated).
 */
bool EqualsMaterialName(const char* name1, const char* name2) {
    return strncmp(name1, name2, 20) == 0;
} // 0x803C1F60

/*
 * TestFileHeader
 *
 * Whether a layout file is big endian (byte-order mark U+FEFF) and of format
 * version 8.
 */
bool TestFileHeader(const res::BinaryFileHeader& fileHeader) {
    return fileHeader.byteOrder == NW4HBM_BYTEORDER_BIG && fileHeader.version == 8;
} // 0x803C1F90

/*
 * TestFileHeader
 *
 * As above, and the file's signature is `testSig`.
 */
bool TestFileHeader(const res::BinaryFileHeader& fileHeader, u32 testSig) {
    return static_cast<u32>(GetSignatureInt(fileHeader.signature)) == testSig && TestFileHeader(fileHeader);
} // 0x803C1FC0

/*
 * TexCoordAry::TexCoordAry
 */
TexCoordAry::TexCoordAry() : mCap(0), mNum(0), mpData(NULL) {
} // 0x803C2000

/*
 * TexCoordAry::Free
 */
void TexCoordAry::Free() {
    if (mpData) {
        Layout::FreeMemory(mpData);
        mpData = NULL;

        mCap = 0;
        mNum = 0;
    }
} // 0x803C2020

/*
 * TexCoordAry::Reserve
 *
 * Makes room for `num` coordinate sets; existing sets are discarded when the
 * storage grows.
 */
void TexCoordAry::Reserve(u8 num) {
    if (mCap < num) {
        Free();
        mpData = static_cast<TexCoords*>(Layout::AllocMemory(sizeof(*mpData) * num));

        if (mpData) {
            mCap = num;
        }
    }
} // 0x803C2070

/*
 * TexCoordAry::SetSize
 *
 * Sets the number of coordinate sets; sets added at the end cover the whole
 * texture.
 */
void TexCoordAry::SetSize(u8 num) {
    if (mpData && num <= mCap) {
        static TexCoords texCoords = {
            math::VEC2(0.0f, 0.0f),
            math::VEC2(1.0f, 0.0f),
            math::VEC2(0.0f, 1.0f),
            math::VEC2(1.0f, 1.0f),
        };

        for (int j = mNum; j < num; ++j) {
            for (int i = 0; i < (int)(sizeof(mpData[j]) / sizeof(mpData[j][0])); ++i) {
                mpData[j][i] = texCoords[i];
            }
        }

        mNum = num;
    }
} // 0x803C2100

/*
 * TexCoordAry::Copy
 *
 * Copies `texCoordNum` coordinate sets from a resource.
 */
void TexCoordAry::Copy(const void* pResTexCoord, u8 texCoordNum) {
    mNum = ut::Max(mNum, texCoordNum);
    const TexCoords* src = static_cast<const TexCoords*>(pResTexCoord);

    for (int j = 0; j < texCoordNum; ++j) {
        for (int i = 0; i < (int)(sizeof(mpData[j]) / sizeof(mpData[j][0])); ++i) {
            mpData[j][i] = src[j][i];
        }
    }
} // 0x803C2200

/*
 * IsModulateVertexColor
 *
 * Whether drawing must modulate by vertex colour: the global alpha is not
 * opaque, or a vertex colour is not opaque white.
 */
bool IsModulateVertexColor(ut::Color* vtxColors, u8 glbAlpha) {
    if (glbAlpha != 0xFF) {
        return true;
    }

    if (vtxColors) {
        if (vtxColors[0] != ut::Color::WHITE || vtxColors[1] != ut::Color::WHITE ||
            vtxColors[2] != ut::Color::WHITE || vtxColors[3] != ut::Color::WHITE) {
            return true;
        }
    }

    return false;
} // 0x803C22A0

/*
 * MultipleAlpha
 *
 * `col` with its alpha scaled by `alpha` / 255.
 */
ut::Color MultipleAlpha(const ut::Color col, u8 alpha) {
    ut::Color ret = col;

    if (alpha != 0xFF) {
        ret.a = col.a * alpha / 255;
    }

    return ret;
} // 0x803C2310

/*
 * MultipleAlpha
 *
 * The four vertex colours of `src`, alpha-scaled into `dst`.
 */
void MultipleAlpha(ut::Color* dst, const ut::Color* src, u8 alpha) {
    for (int i = 0; i < 4; ++i) {
        dst[i] = MultipleAlpha(src[i], alpha);
    }
} // no copy in retail (inlined)

/*
 * SetVertexFormat
 *
 * Describes the vertices DrawQuad sends: a 2D position, a colour when
 * modulating, then `texCoordNum` texture coordinates.
 */
void SetVertexFormat(bool bModulate, u8 texCoordNum) {
    GXClearVtxDesc();

    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (bModulate) {
        GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXSetVtxDesc(static_cast<GXAttr>(GX_VA_TEX0 + i), GX_DIRECT);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGB, GX_F32, 0);

    if (bModulate) {
        GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXSetVtxAttrFmt(GX_VTXFMT0, static_cast<GXAttr>(GX_VA_TEX0 + i), GX_CLR_RGBA, GX_F32, 0);
    }
} // 0x803C2350

/*
 * DrawQuad
 *
 * Draws the rectangle of `size` at `basePt`, starting at the top left and
 * going clockwise; vertex colours and texture coordinates are in top-left,
 * top-right, bottom-left, bottom-right order.
 */
void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);

    GXPosition2f32(basePt.x, basePt.y);

    if (vtxColors) {
        GXColor1u32(vtxColors[0]);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXTexCoord2f32(texCoords[i][0].x, texCoords[i][0].y);
    }

    GXPosition2f32(basePt.x + size.width, basePt.y);

    if (vtxColors) {
        GXColor1u32(vtxColors[1]);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXTexCoord2f32(texCoords[i][1].x, texCoords[i][1].y);
    }

    GXPosition2f32(basePt.x + size.width, basePt.y + size.height);

    if (vtxColors) {
        GXColor1u32(vtxColors[3]);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXTexCoord2f32(texCoords[i][3].x, texCoords[i][3].y);
    }

    GXPosition2f32(basePt.x, basePt.y + size.height);

    if (vtxColors) {
        GXColor1u32(vtxColors[2]);
    }

    for (int i = 0; i < texCoordNum; ++i) {
        GXTexCoord2f32(texCoords[i][2].x, texCoords[i][2].y);
    }

    GXEnd();
} // 0x803C2440

/*
 * DrawQuad
 *
 * As above, with the vertex colours' alpha scaled by `alpha`.
 */
void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors, u8 alpha) {
    ut::Color wkVtxColors[4];

    if (vtxColors) {
        MultipleAlpha(wkVtxColors, vtxColors, alpha);
    }

    DrawQuad(basePt, size, texCoordNum, texCoords, vtxColors ? wkVtxColors : NULL);
} // 0x803C29A0

/*
 * DrawLine
 *
 * Draws the outline of the rectangle of `size` at `pos` in `color`, with its
 * own GX vertex, channel, TEV and blend setup.
 */
void DrawLine(const math::VEC2& pos, const Size& size, ut::Color color) {
    GXClearVtxDesc();

    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);

    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, false, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);

    GXSetChanMatColor(GX_COLOR0A0, color);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetTevDirect(GX_TEVSTAGE0);

    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);

    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);

    GXSetLineWidth(6, GX_TO_ZERO);

    // Top left, then clockwise back to the top left.
    GXBegin(GX_LINESTRIP, GX_VTXFMT0, 5);
    GXPosition2f32(pos.x, pos.y);
    GXPosition2f32(pos.x + size.width, pos.y);
    GXPosition2f32(pos.x + size.width, pos.y + size.height);
    GXPosition2f32(pos.x, pos.y + size.height);
    GXPosition2f32(pos.x, pos.y);
    GXEnd();
} // 0x803C2AE0

/*
 * InitGXTexObjFromTPL
 *
 * Initialises `to` from texture `id` of a TPL palette, binding the palette
 * first if it still holds file offsets.
 */
void InitGXTexObjFromTPL(GXTexObj* to, TPLPalette* pal, u32 id) {
    // An unbound palette holds offsets into the file, not pointers into main memory.
    if (pal->descriptorArray < (TPLDescriptor*)0x80000000) {
        TPLBind(pal);
    }

    TPLDescriptor* tdp = TPLGet(pal, id);

    const GXBool mipMap = tdp->textureHeader->minLOD != tdp->textureHeader->maxLOD ? GX_TRUE : GX_FALSE;

    if (tdp->CLUTHeader) {
        GXInitTexObjCI(to, tdp->textureHeader->data, tdp->textureHeader->width, tdp->textureHeader->height,
                       static_cast<GXCITexFmt>(tdp->textureHeader->format), tdp->textureHeader->wrapS,
                       tdp->textureHeader->wrapT, mipMap, 0);

        GXInitTexObjUserData(to, tdp->CLUTHeader);
    } else {
        GXInitTexObj(to, tdp->textureHeader->data, tdp->textureHeader->width, tdp->textureHeader->height,
                     static_cast<GXTexFmt>(tdp->textureHeader->format), tdp->textureHeader->wrapS,
                     tdp->textureHeader->wrapT, mipMap);
    }

    GXInitTexObjLOD(to, tdp->textureHeader->minFilter, tdp->textureHeader->magFilter, tdp->textureHeader->minLOD,
                    tdp->textureHeader->maxLOD, tdp->textureHeader->LODBias, false, tdp->textureHeader->edgeLODEnable,
                    GX_ANISO_1);
} // 0x803C2C80

} // namespace detail
} // namespace lyt
} // namespace nw4hbm
