#ifndef NW4HBM_LYT_TEXT_BOX_H
#define NW4HBM_LYT_TEXT_BOX_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/textBox.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_textBox.h

#include <revolution/types.h>
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/Rect.h"

namespace nw4hbm {
namespace ut {
class Font;
template <typename CharT> class TagProcessorBase;
template <typename CharT> class TextWriterBase;
namespace detail {
class RuntimeTypeInfo;
} // namespace detail
} // namespace ut

namespace lyt {

struct ResBlockSet;

namespace res {

static const u32 SIGNATURE_TEXT_BOX_BLOCK = 'txt1';

/*
 * res::Font
 *
 * One entry of a layout file's font list: the offset of the font's name.
 */
struct Font {
    u32 nameStrOffset; // 0x00 from the start of the font table
    u8 type;           // 0x04
    u8 padding[3];     // 0x05 // pad: SDK res::Font.padding
}; // size 0x08

/*
 * res::TextBox
 *
 * Layout-file data block of a text box pane ("txt1"): the pane block, the size of the string
 * buffer and of the initial string, its material and font, the text position, the top and
 * bottom text colours and the font metrics. The UTF-16 string follows at `textStrOffset`.
 */
struct TextBox : public Pane {
    u16 textBufBytes;  // 0x4C string buffer, terminator included
    u16 textStrBytes;  // 0x4E initial string, terminator included
    u16 materialIdx;   // 0x50
    u16 fontIdx;       // 0x52
    u8 textPosition;   // 0x54 horizontal + 3 * vertical
    u8 padding[3];     // 0x55 // pad: SDK res::TextBox.padding
    u32 textStrOffset; // 0x58 from the start of the block
    u32 textCols[2];   // 0x5C top, bottom
    Size fontSize;     // 0x64
    f32 charSpace;     // 0x6C
    f32 lineSpace;     // 0x70
}; // size 0x74

} // namespace res

/*
 * TextBox
 *
 * A pane that prints a UTF-16 string with a font, line by line, positioned inside the pane by
 * its text position and coloured with a vertical gradation between two colours.
 */
class TextBox : public Pane {
public:
    TextBox(u16 allocStrLen);
    TextBox(u16 allocStrLen, const wchar_t* str, const ut::Font* pFont);
    TextBox(u16 allocStrLen, const wchar_t* str, u16 strLen, const ut::Font* pFont);
    TextBox(const res::TextBox* pBlock, const ResBlockSet& resBlockSet);
    virtual ~TextBox();

    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const { return &typeInfo; }

    virtual void DrawSelf(const DrawInfo& drawInfo);
    virtual ut::Color GetVtxColor(u32 idx) const;
    virtual void SetVtxColor(u32 idx, ut::Color value);
    virtual u8 GetVtxColorElement(u32 idx) const;
    virtual void SetVtxColorElement(u32 idx, u8 value);

    virtual void AllocStringBuffer(u16 minLen);
    virtual void FreeStringBuffer();
    virtual u16 SetString(const wchar_t* str, u16 dstIdx);
    virtual u16 SetString(const wchar_t* str, u16 dstIdx, u16 strLen);

    ut::Color GetTextColor(u32 type) const { return mTextColors[type]; }
    void SetTextColor(u32 type, ut::Color value) { mTextColors[type] = value; }

    Size GetFontSize() const { return mFontSize; }
    void SetFontSize(const Size& fontSize) { mFontSize = fontSize; }

    void SetTagProcessor(ut::TagProcessorBase<wchar_t>* pTagProcessor) { mpTagProcessor = pTagProcessor; }

    u8 GetTextPositionH() const { return detail::GetHorizontalPosition(mTextPosition); }
    u8 GetTextPositionV() const { return detail::GetVerticalPosition(mTextPosition); }
    void SetTextPositionH(u8 pos) { detail::SetHorizontalPosition(&mTextPosition, pos); }
    void SetTextPositionV(u8 pos) { detail::SetVerticalPosition(&mTextPosition, pos); }

    const ut::Font* GetFont() const;
    void SetFont(const ut::Font* pFont);
    u16 GetStringBufferLength() const;
    ut::Rect GetTextDrawRect(ut::TextWriterBase<wchar_t>* pWriter) const;
    ut::Rect GetTextDrawRect(const DrawInfo& drawInfo) const;
    f32 GetTextMagH() const;
    f32 GetTextMagV() const;

    void Init(u16 allocStrLen);

    static const ut::detail::RuntimeTypeInfo typeInfo;

private:
    wchar_t* mTextBuf;                              // 0xD4
    ut::Color mTextColors[2];                       // 0xD8 top, bottom
    const ut::Font* mpFont;                         // 0xE0
    Size mFontSize;                                 // 0xE4
    f32 mLineSpace;                                 // 0xEC
    f32 mCharSpace;                                 // 0xF0
    ut::TagProcessorBase<wchar_t>* mpTagProcessor;  // 0xF4 NULL: the writer's default
    u16 mTextBufBytes;                              // 0xF8 terminator included
    u16 mTextLen;                                   // 0xFA
    u8 mTextPosition;                               // 0xFC horizontal + 3 * vertical

    struct {
        u8 allocFont : 1; // mpFont was allocated by the text box and is freed with it
    } mTextBoxFlag;                                 // 0xFD
}; // size 0x100

} // namespace lyt
} // namespace nw4hbm

#endif
