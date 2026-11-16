// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_ResFontBase.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_ResFontBase.cpp
#include "nw4hbm/ut/ResFontBase.h"

/*
 * CMapScanEntry
 *
 * One (code, glyph index) pair of a FONT_MAPMETHOD_SCAN code map.
 */
struct CMapScanEntry {
    u16 ccode;                                         // 0x00
    nw4hbm::ut::detail::ResFontBase::GlyphIndex index; // 0x02
}; // size 0x04

/*
 * CMapInfoScan
 *
 * The mapInfo of a FONT_MAPMETHOD_SCAN code map: a count, then the pairs
 * sorted by code.
 */
struct CMapInfoScan {
    u16 num;                 // 0x00
    CMapScanEntry entries[]; // 0x02
}; // size 0x02

namespace nw4hbm {
namespace ut {
namespace detail {

/*
 * ResFontBase::ResFontBase
 *
 * Starts with no resource attached.
 */
ResFontBase::ResFontBase() : mResource(NULL), mFontInfo(NULL) {
} // 0x803CE460

/*
 * ResFontBase::~ResFontBase
 */
ResFontBase::~ResFontBase() {
} // 0x803CE4B0

/*
 * ResFontBase::SetResourceBuffer
 *
 * Attaches the resource `pUserBuffer` with its FINF block `pFontInfo`.
 */
void ResFontBase::SetResourceBuffer(void* pUserBuffer, FontInformation* pFontInfo) {
    mResource = pUserBuffer;
    mFontInfo = pFontInfo;
} // 0x803CE4F0

/*
 * ResFontBase::GetWidth
 */
int ResFontBase::GetWidth() const {
    return mFontInfo->width;
} // 0x803CE500

/*
 * ResFontBase::GetHeight
 */
int ResFontBase::GetHeight() const {
    return mFontInfo->height;
} // 0x803CE510

/*
 * ResFontBase::GetAscent
 */
int ResFontBase::GetAscent() const {
    return mFontInfo->ascent;
} // 0x803CE520

/*
 * ResFontBase::GetDescent
 */
int ResFontBase::GetDescent() const {
    return mFontInfo->height - mFontInfo->ascent;
} // 0x803CE530

/*
 * ResFontBase::GetBaselinePos
 */
int ResFontBase::GetBaselinePos() const {
    return mFontInfo->pGlyph->baselinePos;
} // 0x803CE550

/*
 * ResFontBase::GetCellHeight
 */
int ResFontBase::GetCellHeight() const {
    return mFontInfo->pGlyph->cellHeight;
} // 0x803CE570

/*
 * ResFontBase::GetCellWidth
 */
int ResFontBase::GetCellWidth() const {
    return mFontInfo->pGlyph->cellWidth;
} // 0x803CE580

/*
 * ResFontBase::GetMaxCharWidth
 */
int ResFontBase::GetMaxCharWidth() const {
    return mFontInfo->pGlyph->maxCharWidth;
} // 0x803CE590

/*
 * ResFontBase::GetType
 */
Font::Type ResFontBase::GetType() const {
    return TYPE_RESOURCE;
} // 0x803CE5A0

/*
 * ResFontBase::GetTextureFormat
 */
GXTexFmt ResFontBase::GetTextureFormat() const {
    return static_cast<GXTexFmt>(mFontInfo->pGlyph->sheetFormat);
} // 0x803CE5B0

/*
 * ResFontBase::GetLineFeed
 */
int ResFontBase::GetLineFeed() const {
    return mFontInfo->linefeed;
} // 0x803CE5C0

/*
 * ResFontBase::GetDefaultCharWidths
 */
CharWidths ResFontBase::GetDefaultCharWidths() const {
    return mFontInfo->defaultWidth;
} // 0x803CE5D0

/*
 * ResFontBase::SetDefaultCharWidths
 */
void ResFontBase::SetDefaultCharWidths(const CharWidths& widths) {
    mFontInfo->defaultWidth = widths;
} // 0x803CE5F0

/*
 * ResFontBase::SetAlternateChar
 *
 * Draws codes the font lacks with the glyph of `c`; fails, keeping the old
 * alternate, when the font lacks `c` too.
 */
bool ResFontBase::SetAlternateChar(u16 c) {
    GlyphIndex index = FindGlyphIndex(c);

    if (index != GLYPH_INDEX_NOT_FOUND) {
        mFontInfo->alterCharIndex = index;
        return true;
    } else {
        return false;
    }
} // 0x803CE610

/*
 * ResFontBase::SetLineFeed
 */
void ResFontBase::SetLineFeed(int linefeed) {
    mFontInfo->linefeed = linefeed;
} // 0x803CE6B0

/*
 * ResFontBase::GetCharWidth
 */
int ResFontBase::GetCharWidth(u16 c) const {
    return GetCharWidths(c).charWidth;
} // 0x803CE6C0

/*
 * ResFontBase::GetCharWidths
 */
CharWidths ResFontBase::GetCharWidths(u16 c) const {
    GlyphIndex index = GetGlyphIndex(c);

    return GetCharWidthsFromIndex(index);
} // 0x803CE710

/*
 * ResFontBase::GetGlyph
 */
void ResFontBase::GetGlyph(Glyph* glyph, u16 c) const {
    GlyphIndex index = GetGlyphIndex(c);

    GetGlyphFromIndex(glyph, index);
} // 0x803CE810

/*
 * ResFontBase::GetEncoding
 */
FontEncoding ResFontBase::GetEncoding() const {
    return static_cast<FontEncoding>(mFontInfo->encoding);
} // 0x803CE8C0

/*
 * ResFontBase::GetGlyphIndex
 *
 * The glyph of `c`, or the alternate glyph when the font lacks `c`.
 */
ResFontBase::GlyphIndex ResFontBase::GetGlyphIndex(u16 c) const {
    GlyphIndex index = FindGlyphIndex(c);

    return index != GLYPH_INDEX_NOT_FOUND ? index : mFontInfo->alterCharIndex;
} // no copy in retail (inlined)

/*
 * ResFontBase::FindGlyphIndex
 *
 * Looks `c` up in the code map that covers it.
 */
ResFontBase::GlyphIndex ResFontBase::FindGlyphIndex(u16 c) const {
    FontCodeMap* pMap;

    for (pMap = mFontInfo->pMap; pMap; pMap = pMap->pNext) {
        if (pMap->ccodeBegin <= c && c <= pMap->ccodeEnd) {
            return FindGlyphIndex(pMap, c);
        }
    }

    return GLYPH_INDEX_NOT_FOUND;
} // no copy in retail (inlined)

/*
 * ResFontBase::FindGlyphIndex
 *
 * Looks `c` up in the code map `pMap`, which covers it.
 */
ResFontBase::GlyphIndex ResFontBase::FindGlyphIndex(const FontCodeMap* pMap, u16 c) const {
    GlyphIndex index = GLYPH_INDEX_NOT_FOUND;

    switch (pMap->mappingMethod) {
    case FONT_MAPMETHOD_DIRECT: {
        u16 offset = pMap->mapInfo[0];
        index = c - pMap->ccodeBegin + offset;
    } break;

    case FONT_MAPMETHOD_TABLE: {
        int table_index = c - pMap->ccodeBegin;
        index = pMap->mapInfo[table_index];
    } break;

    case FONT_MAPMETHOD_SCAN: {
        const CMapInfoScan* scanInfo = reinterpret_cast<const CMapInfoScan*>(pMap->mapInfo);
        const CMapScanEntry* first = scanInfo->entries;
        const CMapScanEntry* last = scanInfo->entries + (scanInfo->num - 1);

        while (first <= last) {
            const CMapScanEntry* mid = first + (last - first) / 2;

            if (mid->ccode < c) {
                first = mid + 1;
            } else if (c < mid->ccode) {
                last = mid - 1;
            } else {
                index = mid->index;
                break;
            }
        }
    } break;

    default:
        break;
    }

    return index;
} // 0x803CE8D0

/*
 * ResFontBase::GetCharWidthsFromIndex
 *
 * The widths of glyph `index` from the width block that covers it, or the
 * default widths.
 */
const CharWidths& ResFontBase::GetCharWidthsFromIndex(GlyphIndex index) const {
    const FontWidth* pWidth;

    for (pWidth = mFontInfo->pWidth; pWidth; pWidth = pWidth->pNext) {
        if (pWidth->indexBegin <= index && index <= pWidth->indexEnd) {
            return GetCharWidthsFromIndex(pWidth, index);
        }
    }

    return mFontInfo->defaultWidth;
} // no copy in retail (inlined)

/*
 * ResFontBase::GetCharWidthsFromIndex
 *
 * The widths of glyph `index` from the width block `pWidth`, which covers it.
 */
const CharWidths& ResFontBase::GetCharWidthsFromIndex(const FontWidth* pWidth, GlyphIndex index) const {
    return pWidth->widthTable[index - pWidth->indexBegin];
} // no copy in retail (inlined)

/*
 * ResFontBase::GetGlyphFromIndex
 *
 * Fills `glyph` with the sheet and cell of glyph `index`. Cells are laid out
 * row by row with a one-pixel border, sheetRow cells across and sheetLine
 * down, sheet after sheet.
 */
void ResFontBase::GetGlyphFromIndex(Glyph* glyph, GlyphIndex index) const {
    const FontTextureGlyph& tg = *mFontInfo->pGlyph;

    u32 cellsInASheet = tg.sheetRow * tg.sheetLine;
    u32 sheetNo = index / cellsInASheet;
    u32 cellNo = index % cellsInASheet;
    u32 cellUnitX = cellNo % tg.sheetRow;
    u32 cellUnitY = cellNo / tg.sheetRow;
    u32 cellPixelX = cellUnitX * (tg.cellWidth + 1);
    u32 cellPixelY = cellUnitY * (tg.cellHeight + 1);
    u32 offsetBytes = sheetNo * tg.sheetSize;
    void* pSheet = tg.sheetImage + offsetBytes;

    glyph->pTexture = pSheet;
    glyph->widths = GetCharWidthsFromIndex(index);
    glyph->height = tg.cellHeight;
    glyph->texFormat = static_cast<GXTexFmt>(tg.sheetFormat);
    glyph->texWidth = tg.sheetWidth;
    glyph->texHeight = tg.sheetHeight;
    glyph->cellX = cellPixelX + 1;
    glyph->cellY = cellPixelY + 1;
} // 0x803CE9A0

} // namespace detail
} // namespace ut
} // namespace nw4hbm
