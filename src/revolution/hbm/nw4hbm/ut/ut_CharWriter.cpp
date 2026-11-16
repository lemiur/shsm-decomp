// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_CharWriter.cpp
#include "nw4hbm/ut/CharWriter.h"

#include "nw4hbm/ut/Font.h"

#include <revolution/gx.h>

namespace nw4hbm {

/*
 * SetupGXCommon
 *
 * GX state shared by every CharWriter setup: no fog, identity swap table, one
 * colour channel fed from the vertex colour, one texture coordinate and alpha
 * blending.
 */
static void SetupGXCommon() {
    static ut::Color fog(0x00000000);

    GXSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, fog);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(1);
    GXSetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
    GXSetNumIndStages(0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
} // no copy in retail (inlined)

namespace ut {

CharWriter::LoadingTexture CharWriter::mLoadingTexture;

/*
 * CharWriter::CharWriter
 *
 * Opaque white text at scale 1 with the cursor at the origin, no colour
 * mapping, no gradation and linear filtering.
 */
CharWriter::CharWriter() : mAlpha(0xFF), mIsWidthFixed(false), mFixedWidth(0.0f), mFont(NULL) {
    mLoadingTexture.Reset();
    ResetColorMapping();

    SetGradationMode(GRADMODE_NONE);
    SetTextColor(Color::WHITE);
    SetScale(1.0f, 1.0f);
    SetCursor(0.0f, 0.0f, 0.0f);

    EnableLinearFilter(true, true);
} // 0x803CC430

/*
 * CharWriter::~CharWriter
 */
CharWriter::~CharWriter() {
} // 0x803CC7C0

/*
 * CharWriter::SetFont
 */
void CharWriter::SetFont(const Font& font) {
    mFont = &font;
} // 0x803CC800

/*
 * CharWriter::GetFont
 */
const Font* CharWriter::GetFont() const {
    return mFont;
} // 0x803CC810

/*
 * CharWriter::SetupGX
 *
 * Sets up GX for drawing with the current font: the colour-mapping TEV when a
 * mapping is set, otherwise the setup for the font's texture format.
 */
void CharWriter::SetupGX() {
    ResetTextureCache();

    if (mColorMapping.min != 0x00000000 || mColorMapping.max != Color::WHITE) {
        SetupGXWithColorMapping(mColorMapping.min, mColorMapping.max);
    } else if (mFont) {
        GXTexFmt format = mFont->GetTextureFormat();

        switch (format) {
        case GX_TF_I4:
        case GX_TF_I8:
            SetupGXForI();
            break;

        case GX_TF_IA4:
        case GX_TF_IA8:
            SetupGXDefault();
            break;

        case GX_TF_RGB565:
        case GX_TF_RGB5A3:
        case GX_TF_RGBA8:
            SetupGXForRGBA();
            break;

        default:
            SetupGXDefault();
            break;
        }
    } else {
        SetupGXDefault();
    }
} // 0x803CC820

/*
 * CharWriter::SetColorMapping
 */
void CharWriter::SetColorMapping(Color min, Color max) {
    mColorMapping.min = min;
    mColorMapping.max = max;
} // 0x803CD170

/*
 * CharWriter::ResetColorMapping
 */
void CharWriter::ResetColorMapping() {
    SetColorMapping(0x00000000, Color::WHITE);
} // no copy in retail (inlined)

/*
 * CharWriter::ResetTextureCache
 */
void CharWriter::ResetTextureCache() {
    mLoadingTexture.Reset();
} // no copy in retail (inlined)

/*
 * CharWriter::SetGradationMode
 */
void CharWriter::SetGradationMode(GradationMode mode) {
    mTextColor.gradationMode = mode;

    UpdateVertexColor();
} // 0x803CD1C0

/*
 * CharWriter::SetTextColor
 */
void CharWriter::SetTextColor(Color color) {
    mTextColor.start = color;

    UpdateVertexColor();
} // no copy in retail (inlined)

/*
 * CharWriter::SetTextColor
 */
void CharWriter::SetTextColor(Color start, Color end) {
    mTextColor.start = start;
    mTextColor.end = end;

    UpdateVertexColor();
} // 0x803CD310

/*
 * CharWriter::SetScale
 */
void CharWriter::SetScale(f32 hScale, f32 vScale) {
    mScale.x = hScale;
    mScale.y = vScale;
} // no copy in retail (inlined)

/*
 * CharWriter::GetScaleH
 */
f32 CharWriter::GetScaleH() const {
    return mScale.x;
} // 0x803CD490

/*
 * CharWriter::GetScaleV
 */
f32 CharWriter::GetScaleV() const {
    return mScale.y;
} // 0x803CD4A0

/*
 * CharWriter::SetFontSize
 *
 * Scales the font so a cell is `width` by `height`.
 */
void CharWriter::SetFontSize(f32 width, f32 height) {
    SetScale(width / mFont->GetWidth(), height / mFont->GetHeight());
} // 0x803CD4B0

/*
 * CharWriter::GetFontWidth
 */
f32 CharWriter::GetFontWidth() const {
    return mFont->GetWidth() * mScale.x;
} // 0x803CD580

/*
 * CharWriter::GetFontHeight
 */
f32 CharWriter::GetFontHeight() const {
    return mFont->GetHeight() * mScale.y;
} // 0x803CD5F0

/*
 * CharWriter::GetFontAscent
 */
f32 CharWriter::GetFontAscent() const {
    return mFont->GetAscent() * mScale.y;
} // 0x803CD660

/*
 * CharWriter::EnableLinearFilter
 */
void CharWriter::EnableLinearFilter(bool atSmall, bool atLarge) {
    mFilter.atSmall = atSmall ? GX_LINEAR : GX_NEAR;
    mFilter.atLarge = atLarge ? GX_LINEAR : GX_NEAR;
} // no copy in retail (inlined)

/*
 * CharWriter::IsWidthFixed
 */
bool CharWriter::IsWidthFixed() const {
    return mIsWidthFixed;
} // 0x803CD6D0

/*
 * CharWriter::GetFixedWidth
 */
f32 CharWriter::GetFixedWidth() const {
    return mFixedWidth;
} // 0x803CD6E0

/*
 * CharWriter::Print
 *
 * Draws `code` at the cursor and advances the cursor by its width (the fixed
 * width, with the glyph centred in it, when fixed width is on); returns the
 * advance.
 */
f32 CharWriter::Print(u16 code) {
    Glyph glyph;
    f32 width;
    f32 left;

    mFont->GetGlyph(&glyph, code);

    CharWidths& widths = glyph.widths;

    if (mIsWidthFixed) {
        f32 margin = (mFixedWidth - widths.charWidth * mScale.x) / 2.0f;

        width = mFixedWidth;
        left = margin + widths.left * mScale.x;
    } else {
        width = widths.charWidth * mScale.x;
        left = widths.left * mScale.x;
    }

    PrintGlyph(mCursorPos.x + left, mCursorPos.y, mCursorPos.z, glyph);

    mCursorPos.x += width;
    return width;
} // 0x803CD6F0

/*
 * CharWriter::SetCursor
 */
void CharWriter::SetCursor(f32 x, f32 y) {
    mCursorPos.x = x;
    mCursorPos.y = y;
} // 0x803CD830

/*
 * CharWriter::SetCursor
 */
void CharWriter::SetCursor(f32 x, f32 y, f32 z) {
    mCursorPos.x = x;
    mCursorPos.y = y;
    mCursorPos.z = z;
} // no copy in retail (inlined)

/*
 * CharWriter::SetCursorX
 */
void CharWriter::SetCursorX(f32 x) {
    mCursorPos.x = x;
} // 0x803CD840

/*
 * CharWriter::SetCursorY
 */
void CharWriter::SetCursorY(f32 y) {
    mCursorPos.y = y;
} // 0x803CD850

/*
 * CharWriter::MoveCursorX
 */
void CharWriter::MoveCursorX(f32 dx) {
    mCursorPos.x += dx;
} // 0x803CD860

/*
 * CharWriter::MoveCursorY
 */
void CharWriter::MoveCursorY(f32 dy) {
    mCursorPos.y += dy;
} // 0x803CD870

/*
 * CharWriter::GetCursorX
 */
f32 CharWriter::GetCursorX() const {
    return mCursorPos.x;
} // 0x803CD880

/*
 * CharWriter::GetCursorY
 */
f32 CharWriter::GetCursorY() const {
    return mCursorPos.y;
} // 0x803CD890

/*
 * CharWriter::PrintGlyph
 *
 * Draws `glyph` as one quad with its upper-left corner at (x, y, z), texture
 * coordinates in 1.15 fixed point.
 */
void CharWriter::PrintGlyph(f32 x, f32 y, f32 z, const Glyph& glyph) {
    f32 posLeft = x;
    f32 posTop = y;
    f32 posRight = posLeft + glyph.widths.glyphWidth * mScale.x;
    f32 posBottom = posTop + glyph.height * mScale.y;
    f32 posZ = z;
    u16 texLeft = static_cast<u32>(glyph.cellX << 15) / glyph.texWidth;
    u16 texTop = static_cast<u32>(glyph.cellY << 15) / glyph.texHeight;
    u16 texRight = static_cast<u32>((glyph.cellX + glyph.widths.glyphWidth) << 15) / glyph.texWidth;
    u16 texBottom = static_cast<u32>((glyph.cellY + glyph.height) << 15) / glyph.texHeight;

    LoadTexture(glyph, GX_TEXMAP0);

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(posLeft, posTop, posZ);
    GXColor1u32(mVertexColor.lu);
    GXTexCoord2u16(texLeft, texTop);

    GXPosition3f32(posRight, posTop, posZ);
    GXColor1u32(mVertexColor.ru);
    GXTexCoord2u16(texRight, texTop);

    GXPosition3f32(posRight, posBottom, posZ);
    GXColor1u32(mVertexColor.rd);
    GXTexCoord2u16(texRight, texBottom);

    GXPosition3f32(posLeft, posBottom, posZ);
    GXColor1u32(mVertexColor.ld);
    GXTexCoord2u16(texLeft, texBottom);
    GXEnd();
} // 0x803CD8A0

/*
 * CharWriter::LoadTexture
 *
 * Loads the glyph's sheet into `slot` unless it is the sheet loaded last.
 */
void CharWriter::LoadTexture(const Glyph& glyph, GXTexMapID slot) {
    LoadingTexture loadInfo;
    loadInfo.slot = slot;
    loadInfo.texture = glyph.pTexture;
    loadInfo.filter = mFilter;

    if (loadInfo != mLoadingTexture) {
        GXTexObj tobj;

        GXInitTexObj(&tobj, glyph.pTexture, glyph.texWidth, glyph.texHeight, glyph.texFormat, GX_CLAMP, GX_CLAMP,
                     GX_FALSE);
        GXInitTexObjLOD(&tobj, mFilter.atSmall, mFilter.atLarge, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
        GXLoadTexObj(&tobj, slot);

        mLoadingTexture = loadInfo;
    }
} // no copy in retail (inlined)

/*
 * CharWriter::UpdateVertexColor
 *
 * Spreads the text colours over the quad's corners by the gradation mode and
 * multiplies their alpha by mAlpha.
 */
void CharWriter::UpdateVertexColor() {
    mVertexColor.lu = mTextColor.start;
    mVertexColor.ru = mTextColor.gradationMode != GRADMODE_H ? mTextColor.start : mTextColor.end;
    mVertexColor.ld = mTextColor.gradationMode != GRADMODE_V ? mTextColor.start : mTextColor.end;
    mVertexColor.rd = mTextColor.gradationMode == GRADMODE_NONE ? mTextColor.start : mTextColor.end;

    mVertexColor.lu.a = mVertexColor.lu.a * mAlpha / 255;
    mVertexColor.ru.a = mVertexColor.ru.a * mAlpha / 255;
    mVertexColor.ld.a = mVertexColor.ld.a * mAlpha / 255;
    mVertexColor.rd.a = mVertexColor.rd.a * mAlpha / 255;
} // no copy in retail (inlined)

/*
 * CharWriter::SetupVertexFormat
 *
 * Vertex format 0: f32 xyz position, RGBA8 colour, u16 1.15 texture
 * coordinates, all direct.
 */
void CharWriter::SetupVertexFormat() {
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 15);

    GXClearVtxDesc();

    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
} // no copy in retail (inlined)

/*
 * CharWriter::SetupGXDefault
 *
 * One TEV stage modulating the texture by the vertex colour.
 */
void CharWriter::SetupGXDefault() {
    SetupGXCommon();

    GXSetNumTevStages(1);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);

    SetupVertexFormat();
} // no copy in retail (inlined)

/*
 * CharWriter::SetupGXWithColorMapping
 *
 * Two TEV stages: the texture is mapped from [min, max], then modulated by the
 * vertex colour.
 */
void CharWriter::SetupGXWithColorMapping(Color min, Color max) {
    SetupGXCommon();

    GXSetNumTevStages(2);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevDirect(GX_TEVSTAGE1);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColor(GX_TEVREG0, min);
    GXSetTevColor(GX_TEVREG1, max);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A0, GX_CA_A1, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_RASA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    SetupVertexFormat();
} // 0x803CDB30

/*
 * CharWriter::SetupGXForI
 *
 * One TEV stage for intensity sheets: colour from the vertex, alpha the
 * texture intensity times the vertex alpha.
 */
void CharWriter::SetupGXForI() {
    SetupGXCommon();

    GXSetNumTevStages(1);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);

    SetupVertexFormat();
} // no copy in retail (inlined)

/*
 * CharWriter::SetupGXForRGBA
 */
void CharWriter::SetupGXForRGBA() {
    SetupGXDefault();
} // no copy in retail (inlined)

} // namespace ut
} // namespace nw4hbm
