#ifndef NW4HBM_UT_RES_FONT_BASE_H
#define NW4HBM_UT_RES_FONT_BASE_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ResFontBase.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_ResFontBase.h

#include <revolution/types.h>
#include "nw4hbm/ut/Font.h"

namespace nw4hbm {
namespace ut {
namespace detail {

/*
 * ResFontBase
 *
 * Font served from a font resource already in memory: every query reads the
 * resource's FontInformation and the blocks it points to.
 */
class ResFontBase : public Font {
public:
    typedef u16 GlyphIndex;
    static const GlyphIndex GLYPH_INDEX_NOT_FOUND = 0xFFFF;

    ResFontBase();
    virtual ~ResFontBase();

    virtual int GetWidth() const;
    virtual int GetHeight() const;
    virtual int GetAscent() const;
    virtual int GetDescent() const;
    virtual int GetBaselinePos() const;
    virtual int GetCellHeight() const;
    virtual int GetCellWidth() const;
    virtual int GetMaxCharWidth() const;
    virtual Type GetType() const;
    virtual GXTexFmt GetTextureFormat() const;
    virtual int GetLineFeed() const;
    virtual CharWidths GetDefaultCharWidths() const;
    virtual void SetDefaultCharWidths(const CharWidths& widths);
    virtual bool SetAlternateChar(u16 c);
    virtual void SetLineFeed(int linefeed);
    virtual int GetCharWidth(u16 c) const;
    virtual CharWidths GetCharWidths(u16 c) const;
    virtual void GetGlyph(Glyph* glyph, u16 c) const;
    virtual FontEncoding GetEncoding() const;

    GlyphIndex GetGlyphIndex(u16 c) const;
    const CharWidths& GetCharWidthsFromIndex(GlyphIndex index) const;
    const CharWidths& GetCharWidthsFromIndex(const FontWidth* pWidth, GlyphIndex index) const;
    void GetGlyphFromIndex(Glyph* glyph, GlyphIndex index) const;
    bool IsManaging(const void* ptr) const { return mResource == ptr; }

    void SetResourceBuffer(void* pUserBuffer, FontInformation* pFontInfo);
    void* RemoveResourceBuffer();

    GlyphIndex FindGlyphIndex(u16 c) const;
    GlyphIndex FindGlyphIndex(const FontCodeMap* pMap, u16 c) const;

private:
    void* mResource;            // 0x10 the font resource, as the user passed it
    FontInformation* mFontInfo; // 0x14 its FINF block
}; // size 0x18

} // namespace detail
} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_RES_FONT_BASE_H
