#ifndef NW4HBM_UT_CHAR_WRITER_H
#define NW4HBM_UT_CHAR_WRITER_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/CharWriter.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_CharWriter.h

#include <revolution/types.h>
#include <revolution/gx/GXEnum.h>
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"

namespace nw4hbm {
namespace ut {

class Font;
struct Glyph;

/*
 * CharWriter
 *
 * Draws single characters of a Font with GX: holds the cursor, the scale,
 * the text colours (with an optional horizontal or vertical gradation) and the
 * colour mapping applied to the glyph sheets.
 */
class CharWriter {
public:
    /*
     * GradationMode
     *
     * How the start and end text colours are spread over a glyph.
     */
    enum GradationMode {
        GRADMODE_NONE,
        GRADMODE_H,
        GRADMODE_V,

        NUM_OF_GRADMODE
    };

private:
    /*
     * ColorMapping
     *
     * Texel intensity 0 maps to `min`, full intensity to `max`.
     */
    struct ColorMapping {
        Color min; // 0x00
        Color max; // 0x04
    };

    /*
     * VertexColor
     *
     * Colours of the four corners of a glyph quad.
     */
    struct VertexColor {
        Color lu; // 0x00 left, upper
        Color ru; // 0x04 right, upper
        Color ld; // 0x08 left, lower
        Color rd; // 0x0C right, lower
    };

    /*
     * TextColor
     */
    struct TextColor {
        Color start;                 // 0x00
        Color end;                   // 0x04
        GradationMode gradationMode; // 0x08
    };

    /*
     * TextureFilter
     *
     * GX minification and magnification filters of the glyph sheets.
     */
    struct TextureFilter {
        bool operator!=(const TextureFilter& rhs) const {
            return atSmall != rhs.atSmall || atLarge != rhs.atLarge;
        }

        GXTexFilter atSmall; // 0x00
        GXTexFilter atLarge; // 0x04
    };

    /*
     * LoadingTexture
     *
     * The glyph sheet last loaded into a texture map slot; a glyph on the same
     * sheet with the same filter is drawn without reloading it.
     */
    struct LoadingTexture {
        bool operator!=(const LoadingTexture& rhs) const {
            return slot != rhs.slot || texture != rhs.texture || filter != rhs.filter;
        }

        void Reset() {
            slot = GX_TEXMAP_NULL;
            texture = NULL;
        }

        GXTexMapID slot;      // 0x00
        void* texture;        // 0x04
        TextureFilter filter; // 0x08
    };

public:
    CharWriter();
    ~CharWriter();

    Color GetColorMappingMin() const;
    Color GetColorMappingMax() const;
    Color GetTextColor() const;
    Color GetGradationStartColor() const;
    Color GetGradationEndColor() const;
    GradationMode GetGradationMode() const;
    f32 GetScaleH() const;
    f32 GetScaleV() const;
    f32 GetCursorX() const;
    f32 GetCursorY() const;
    f32 GetCursorZ() const;
    bool IsLinearFilterEnableAtSmall() const;
    bool IsLinearFilterEnableAtLarge() const;
    u8 GetAlpha() const;
    const Font* GetFont() const;

    void SetColorMapping(Color min, Color max);
    void SetScale(f32 hScale, f32 vScale);
    void SetScale(f32 scale);
    void SetCursor(f32 x, f32 y);
    void SetCursor(f32 x, f32 y, f32 z);
    void MoveCursor(f32 dx, f32 dy);
    void MoveCursor(f32 dx, f32 dy, f32 dz);
    void SetCursorX(f32 x);
    void SetCursorY(f32 y);
    void SetCursorZ(f32 z);
    void MoveCursorX(f32 dx);
    void MoveCursorY(f32 dy);
    void MoveCursorZ(f32 dz);
    void EnableLinearFilter(bool atSmall, bool atLarge);
    void EnableFixedWidth(bool isFixed);
    void SetFixedWidth(f32 width);
    void SetFont(const Font& font);

    f32 GetFontWidth() const;
    f32 GetFontHeight() const;
    f32 GetFontAscent() const;
    f32 GetFontDescent() const;
    bool IsWidthFixed() const;
    f32 GetFixedWidth() const;

    void SetGradationMode(GradationMode mode);
    void SetTextColor(Color color);
    void SetTextColor(Color start, Color end);
    void SetFontSize(f32 width, f32 height);
    void SetFontSize(f32 height);
    void SetAlpha(u8 alpha);

    void SetupGX();
    void ResetColorMapping();
    void ResetTextureCache();
    f32 Print(u16 code);
    void DrawGlyph(const Glyph& glyph);
    void PrintGlyph(f32 x, f32 y, f32 z, const Glyph& glyph);
    void LoadTexture(const Glyph& glyph, GXTexMapID slot);
    void UpdateVertexColor();

private:
    static void SetupVertexFormat();

    static void SetupGXDefault();
    static void SetupGXWithColorMapping(Color min, Color max);
    static void SetupGXForI();
    static void SetupGXForRGBA();

    static LoadingTexture mLoadingTexture;

    ColorMapping mColorMapping; // 0x00
    VertexColor mVertexColor;   // 0x08
    TextColor mTextColor;       // 0x18
    math::VEC2 mScale;          // 0x24
    math::VEC3 mCursorPos;      // 0x2C
    TextureFilter mFilter;      // 0x38
    u8 padding_[2];             // 0x40 // pad: SDK CharWriter.padding_
    u8 mAlpha;                  // 0x42
    bool mIsWidthFixed;         // 0x43
    f32 mFixedWidth;            // 0x44
    const Font* mFont;          // 0x48
};

} // namespace ut
} // namespace nw4hbm

#endif
