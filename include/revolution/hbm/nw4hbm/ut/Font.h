#ifndef NW4HBM_UT_FONT_H
#define NW4HBM_UT_FONT_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/Font.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_Font.h

#include <revolution/types.h>
#include <revolution/gx/GXEnum.h>
#include "nw4hbm/ut/CharStrmReader.h"

namespace nw4hbm {
namespace ut {

/*
 * FontEncoding
 *
 * Character encoding of a font's strings; selects the CharStrmReader decoder.
 */
enum FontEncoding {
    FONT_ENCODING_UTF8,
    FONT_ENCODING_UTF16,
    FONT_ENCODING_SJIS,
    FONT_ENCODING_CP1252,

    NUM_OF_FONT_ENCODING
};

/*
 * CharWidths
 *
 * Horizontal metrics of one character, in pixels.
 */
struct CharWidths {
    s8 left;       // 0x00 space left of the glyph
    u8 glyphWidth; // 0x01
    s8 charWidth;  // 0x02 advance
};

/*
 * FontMapMethod
 *
 * How a FontCodeMap block turns a character code into a glyph index.
 */
typedef u16 FontMapMethod;
enum FontMapMethod_et {
    FONT_MAPMETHOD_DIRECT, // index = mapInfo[0] + (code - ccodeBegin)
    FONT_MAPMETHOD_TABLE,  // index = mapInfo[code - ccodeBegin]
    FONT_MAPMETHOD_SCAN    // binary search of (code, index) pairs
};

/*
 * FontCodeMap
 *
 * One CMAP block of a font resource: the glyph indices of the codes
 * ccodeBegin..ccodeEnd. The blocks form a list through pNext.
 */
struct FontCodeMap {
    u16 ccodeBegin;              // 0x00
    u16 ccodeEnd;                // 0x02
    FontMapMethod mappingMethod; // 0x04
    u16 reserved;                // 0x06
    FontCodeMap* pNext;          // 0x08
    u16 mapInfo[];               // 0x0C layout depends on mappingMethod
}; // size 0x0C

/*
 * FontTextureGlyph
 *
 * The TGLP block of a font resource: the glyph sheets and their cell grid.
 */
struct FontTextureGlyph {
    u8 cellWidth;    // 0x00
    u8 cellHeight;   // 0x01
    s8 baselinePos;  // 0x02
    u8 maxCharWidth; // 0x03
    u32 sheetSize;   // 0x04 bytes per sheet
    u16 sheetNum;    // 0x08
    u16 sheetFormat; // 0x0A GXTexFmt
    u16 sheetRow;    // 0x0C cells per row
    u16 sheetLine;   // 0x0E rows per sheet
    u16 sheetWidth;  // 0x10
    u16 sheetHeight; // 0x12
    u8* sheetImage;  // 0x14 first sheet; the others follow
}; // size 0x18

/*
 * FontWidth
 *
 * One CWDH block of a font resource: the widths of the glyphs
 * indexBegin..indexEnd. The blocks form a list through pNext.
 */
struct FontWidth {
    u16 indexBegin;          // 0x00
    u16 indexEnd;            // 0x02
    FontWidth* pNext;        // 0x04
    CharWidths widthTable[]; // 0x08
}; // size 0x08

/*
 * FontInformation
 *
 * The FINF block of a font resource: global metrics and the heads of the
 * glyph, width and code-map blocks.
 */
struct FontInformation {
    u8 fontType;              // 0x00
    s8 linefeed;              // 0x01
    u16 alterCharIndex;       // 0x02 glyph drawn for codes the font lacks
    CharWidths defaultWidth;  // 0x04
    u8 encoding;              // 0x07 FontEncoding
    FontTextureGlyph* pGlyph; // 0x08
    FontWidth* pWidth;        // 0x0C
    FontCodeMap* pMap;        // 0x10
    u8 height;                // 0x14
    u8 width;                 // 0x15
    u8 ascent;                // 0x16
    u8 padding_[1];           // 0x17 // pad: SDK FontInformation.padding_
}; // size 0x18

/*
 * Glyph
 *
 * Where one character's image sits in its font texture.
 */
struct Glyph {
    void* pTexture;    // 0x00
    CharWidths widths; // 0x04
    u8 height;         // 0x07
    GXTexFmt texFormat; // 0x08
    u16 texWidth;      // 0x0C
    u16 texHeight;     // 0x0E
    u16 cellX;         // 0x10
    u16 cellY;         // 0x12
};

/*
 * Font
 *
 * Abstract font: metrics, glyph lookup and the decoder for its encoding.
 */
class Font {
public:
    enum Type {
        TYPE_NULL,
        TYPE_ROM,
        TYPE_RESOURCE
    };

    Font() : mReaderFunc(&CharStrmReader::ReadNextCharCP1252) {}
    virtual ~Font() {}

    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    virtual int GetAscent() const = 0;
    virtual int GetDescent() const = 0;
    virtual int GetBaselinePos() const = 0;
    virtual int GetCellHeight() const = 0;
    virtual int GetCellWidth() const = 0;
    virtual int GetMaxCharWidth() const = 0;
    virtual Type GetType() const = 0;
    virtual GXTexFmt GetTextureFormat() const = 0;
    virtual int GetLineFeed() const = 0;
    virtual CharWidths GetDefaultCharWidths() const = 0;
    virtual void SetDefaultCharWidths(const CharWidths& widths) = 0;
    virtual bool SetAlternateChar(u16 c) = 0;
    virtual void SetLineFeed(int linefeed) = 0;
    virtual int GetCharWidth(u16 c) const = 0;
    virtual CharWidths GetCharWidths(u16 c) const = 0;
    virtual void GetGlyph(Glyph* glyph, u16 c) const = 0;
    virtual FontEncoding GetEncoding() const = 0;

    void InitReaderFunc(FontEncoding encoding);

    CharStrmReader GetCharStrmReader() const {
        CharStrmReader reader(mReaderFunc);
        return reader;
    }

private:
    CharStrmReader::ReadFunc mReaderFunc; // 0x04 decoder for GetEncoding()
};

} // namespace ut
} // namespace nw4hbm

#endif
