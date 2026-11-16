// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_TextWriterBase.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_TextWriterBase.cpp
#include "nw4hbm/ut/TextWriterBase.h"

#include "nw4hbm/ut/CharStrmReader.h"
#include "nw4hbm/ut/CharWriter.h"
#include "nw4hbm/ut/Font.h"
#include "nw4hbm/ut/Rect.h"
#include "nw4hbm/ut/TagProcessorBase.h"
#include "nw4hbm/ut/inlines.h"

#include <stdarg.h>

namespace nw4hbm {
namespace ut {

// .data
template <typename CharT>
u32 TextWriterBase<CharT>::mFormatBufferSize = 0x100;

// .bss
template <typename CharT>
CharT* TextWriterBase<CharT>::mFormatBuffer;

template <typename CharT>
TagProcessorBase<CharT> TextWriterBase<CharT>::mDefaultTagProcessor;

/*
 * TextWriterBase::TextWriterBase
 */
template <typename CharT>
TextWriterBase<CharT>::TextWriterBase()
    : mCharSpace(0.0f), mLineSpace(0.0f), mTabWidth(4), mDrawFlag(0), mTagProcessor(&mDefaultTagProcessor) {
} // 0x803CF570

/*
 * TextWriterBase::~TextWriterBase
 */
template <typename CharT>
TextWriterBase<CharT>::~TextWriterBase() {
} // 0x803CF5D0

/*
 * TextWriterBase::SetLineHeight
 *
 * Sets the line spacing so that a line of the current font is `lineHeight` tall.
 */
template <typename CharT>
void TextWriterBase<CharT>::SetLineHeight(f32 lineHeight) {
    const Font* font = GetFont();
    int linefeed = font ? font->GetLineFeed() : 0;

    mLineSpace = lineHeight - linefeed * GetScaleV();
} // 0x803CF630

/*
 * TextWriterBase::GetLineHeight
 */
template <typename CharT>
f32 TextWriterBase<CharT>::GetLineHeight() const {
    const Font* font = GetFont();
    int linefeed = font ? font->GetLineFeed() : 0;

    return mLineSpace + linefeed * GetScaleV();
} // 0x803CF6D0

/*
 * TextWriterBase::SetLineSpace
 */
template <typename CharT>
void TextWriterBase<CharT>::SetLineSpace(f32 lineSpace) {
    mLineSpace = lineSpace;
} // 0x803CF760

/*
 * TextWriterBase::SetCharSpace
 */
template <typename CharT>
void TextWriterBase<CharT>::SetCharSpace(f32 charSpace) {
    mCharSpace = charSpace;
} // 0x803CF770

/*
 * TextWriterBase::GetLineSpace
 */
template <typename CharT>
f32 TextWriterBase<CharT>::GetLineSpace() const {
    return mLineSpace;
} // 0x803CF780

/*
 * TextWriterBase::GetCharSpace
 */
template <typename CharT>
f32 TextWriterBase<CharT>::GetCharSpace() const {
    return mCharSpace;
} // 0x803CF790

/*
 * TextWriterBase::SetTabWidth
 */
template <typename CharT>
void TextWriterBase<CharT>::SetTabWidth(int tabWidth) {
    mTabWidth = tabWidth;
} // 0x803CF7A0

/*
 * TextWriterBase::GetTabWidth
 */
template <typename CharT>
int TextWriterBase<CharT>::GetTabWidth() const {
    return mTabWidth;
} // 0x803CF7B0

/*
 * TextWriterBase::SetDrawFlag
 */
template <typename CharT>
void TextWriterBase<CharT>::SetDrawFlag(u32 flags) {
    mDrawFlag = flags;
} // 0x803CF7C0

/*
 * TextWriterBase::GetDrawFlag
 */
template <typename CharT>
u32 TextWriterBase<CharT>::GetDrawFlag() const {
    return mDrawFlag;
} // 0x803CF7D0

/*
 * TextWriterBase::SetTagProcessor
 */
template <typename CharT>
void TextWriterBase<CharT>::SetTagProcessor(TagProcessorBase<CharT>* tagProcessor) {
    mTagProcessor = tagProcessor;
} // 0x803CF7E0

/*
 * TextWriterBase::ResetTagProcessor
 */
template <typename CharT>
void TextWriterBase<CharT>::ResetTagProcessor() {
    mTagProcessor = &mDefaultTagProcessor;
} // 0x803CF7F0

/*
 * TextWriterBase::GetTagProcessor
 */
template <typename CharT>
TagProcessorBase<CharT>& TextWriterBase<CharT>::GetTagProcessor() const {
    return *mTagProcessor;
} // 0x803CF800

/*
 * TextWriterBase::CalcFormatStringWidth
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcFormatStringWidth(const CharT* format, ...) const {
    Rect rect;
    va_list vargs;

    va_start(vargs, format);

    CalcVStringRect(&rect, format, vargs);

    va_end(vargs);

    return rect.GetWidth();
} // 0x803CF810

/*
 * TextWriterBase::CalcFormatStringHeight
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcFormatStringHeight(const CharT* format, ...) const {
    Rect rect;
    va_list vargs;

    va_start(vargs, format);

    CalcVStringRect(&rect, format, vargs);

    va_end(vargs);

    return rect.GetHeight();
} // 0x803CFA10

/*
 * TextWriterBase::CalcFormatStringRect
 */
template <typename CharT>
void TextWriterBase<CharT>::CalcFormatStringRect(Rect* pRect, const CharT* format, ...) const {
    va_list vargs;

    va_start(vargs, format);

    CalcVStringRect(pRect, format, vargs);

    va_end(vargs);
} // 0x803CFC10

/*
 * TextWriterBase::CalcVStringRect
 *
 * Formats into the shared format buffer (or a stack buffer when none was set) and measures the result.
 */
template <typename CharT>
void TextWriterBase<CharT>::CalcVStringRect(Rect* pRect, const CharT* format, va_list args) const {
    CharT* buffer = mFormatBuffer ? mFormatBuffer : static_cast<CharT*>(__alloca(mFormatBufferSize));

    int length = VSNPrintf(buffer, mFormatBufferSize, format, args);
    CalcStringRect(pRect, buffer, length);
} // 0x803CFDE0

/*
 * TextWriterBase::CalcStringWidth
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcStringWidth(const CharT* str, int length) const {
    Rect rect;
    CalcStringRect(&rect, str, length);

    return rect.GetWidth();
} // 0x803CFF50

/*
 * TextWriterBase::CalcStringWidth
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcStringWidth(const CharT* str) const {
    return CalcStringWidth(str, StrLen(str));
} // 0x803D0090

/*
 * TextWriterBase::CalcStringHeight
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcStringHeight(const CharT* str, int length) const {
    Rect rect;
    CalcStringRect(&rect, str, length);

    return rect.GetHeight();
} // 0x803D01E0

/*
 * TextWriterBase::CalcStringHeight
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcStringHeight(const CharT* str) const {
    return CalcStringHeight(str, StrLen(str));
} // 0x803D0320

/*
 * TextWriterBase::CalcStringRect
 *
 * Measures on a copy of the writer, so the caller's cursor is untouched.
 */
template <typename CharT>
void TextWriterBase<CharT>::CalcStringRect(Rect* pRect, const CharT* str, int length) const {
    TextWriterBase<CharT> myCopy = *this;

    myCopy.CalcStringRectImpl(pRect, str, length);
} // 0x803D0470

/*
 * TextWriterBase::CalcStringRect
 */
template <typename CharT>
void TextWriterBase<CharT>::CalcStringRect(Rect* pRect, const CharT* str) const {
    CalcStringRect(pRect, str, StrLen(str));
} // 0x803D0580

/*
 * TextWriterBase::Printf
 */
template <typename CharT>
f32 TextWriterBase<CharT>::Printf(const CharT* format, ...) {
    va_list vargs;

    va_start(vargs, format);

    f32 width = VPrintf(format, vargs);

    va_end(vargs);

    return width;
} // 0x803D06B0

/*
 * TextWriterBase::VPrintf
 *
 * Formats into the shared format buffer (or a stack buffer when none was set) and prints the result.
 */
template <typename CharT>
f32 TextWriterBase<CharT>::VPrintf(const CharT* format, va_list args) {
    CharT* buffer = mFormatBuffer ? mFormatBuffer : static_cast<CharT*>(__alloca(mFormatBufferSize));

    int length = VSNPrintf(buffer, mFormatBufferSize, format, args);
    f32 width = Print(buffer, length);
    return width;
} // 0x803D08E0

/*
 * TextWriterBase::Print
 *
 * Prints on a copy of the writer and moves this writer's cursor to where the copy ended.
 */
template <typename CharT>
f32 TextWriterBase<CharT>::Print(const CharT* str, int length) {
    TextWriterBase<CharT> myCopy = *this;

    f32 width = myCopy.PrintImpl(str, length);
    SetCursor(myCopy.GetCursorX(), myCopy.GetCursorY());

    return width;
} // 0x803D0AB0

/*
 * TextWriterBase::Print
 */
template <typename CharT>
f32 TextWriterBase<CharT>::Print(const CharT* str) {
    return Print(str, StrLen(str));
} // 0x803D0C10

/*
 * TextWriterBase::SetBuffer
 */
template <typename CharT>
CharT* TextWriterBase<CharT>::SetBuffer(CharT* buffer, u32 size) {
    CharT* oldBuffer = mFormatBuffer;

    mFormatBuffer = buffer;
    mFormatBufferSize = size;

    return oldBuffer;
} // 0x803D0D80

/*
 * TextWriterBase::SetBuffer
 *
 * Drops the shared buffer: later formatting uses a stack buffer of `size`.
 */
template <typename CharT>
CharT* TextWriterBase<CharT>::SetBuffer(u32 size) {
    CharT* oldBuffer = mFormatBuffer;

    mFormatBuffer = NULL;
    mFormatBufferSize = size;

    return oldBuffer;
} // 0x803D0DA0

/*
 * TextWriterBase::GetBufferSize
 */
template <typename CharT>
u32 TextWriterBase<CharT>::GetBufferSize() {
    return mFormatBufferSize;
} // 0x803D0DC0

/*
 * TextWriterBase::GetBuffer
 */
template <typename CharT>
CharT* TextWriterBase<CharT>::GetBuffer() {
    return mFormatBuffer;
} // 0x803D0DD0

/*
 * TextWriterBase::CalcLineWidth
 *
 * Width of the first line of `str`, measured on a copy of the writer.
 */
template <typename CharT>
f32 TextWriterBase<CharT>::CalcLineWidth(const CharT* str, int length) {
    Rect rect;
    TextWriterBase<CharT> myCopy = *this;

    myCopy.SetCursor(0.0f, 0.0f);
    myCopy.CalcLineRectImpl(&rect, str, length);

    return rect.GetWidth();
} // 0x803D0E00

/*
 * TextWriterBase::CalcLineRectImpl
 *
 * Bounds of one line; returns the number of characters read.
 */
template <typename CharT>
int TextWriterBase<CharT>::CalcLineRectImpl(Rect* pRect, const CharT* str, int length) {
    PrintContext<CharT> context = {this, str, 0.0f, 0.0f, 0};
    const Font* font = GetFont();
    f32 x = 0.0f;
    bool bCharSpace = false;
    CharStrmReader reader = font->GetCharStrmReader();

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = Min(0.0f, GetLineHeight());
    pRect->bottom = Max(0.0f, GetLineHeight());

    reader.Set(str);

    for (u16 code = reader.Next(); static_cast<const CharT*>(reader.GetCurrentPos()) - str <= length;
         code = reader.Next()) {
        if (code < ' ') {
            Operation operation;
            Rect rect(x, 0.0f, 0.0f, 0.0f);

            context.str = static_cast<const CharT*>(reader.GetCurrentPos());
            context.flags = 0;
            context.flags |= bCharSpace ? 0 : 1 << 0;

            SetCursorX(x);

            operation = mTagProcessor->CalcRect(&rect, code, &context);
            reader.Set(context.str);

            pRect->left = Min(pRect->left, rect.left);
            pRect->top = Min(pRect->top, rect.top);
            pRect->right = Max(pRect->right, rect.right);
            pRect->bottom = Max(pRect->bottom, rect.bottom);

            x = GetCursorX();

            if (operation == OPERATION_END_DRAW) {
                return length;
            } else if (operation == OPERATION_NO_CHAR_SPACE) {
                bCharSpace = false;
            } else if (operation == OPERATION_CHAR_SPACE) {
                bCharSpace = true;
            } else if (operation == OPERATION_NEXT_LINE) {
                break;
            }
        } else {
            if (bCharSpace) {
                x += GetCharSpace();
            }

            bCharSpace = true;

            if (IsWidthFixed()) {
                x += GetFixedWidth();
            } else {
                x += GetFont()->GetCharWidth(code) * GetScaleH();
            }

            pRect->left = Min(pRect->left, x);
            pRect->right = Max(pRect->right, x);
        }
    }

    return static_cast<const CharT*>(reader.GetCurrentPos()) - str;
} // 0x803D0F70

/*
 * TextWriterBase::CalcStringRectImpl
 *
 * Bounds of the whole string: the union of the bounds of its lines.
 */
template <typename CharT>
void TextWriterBase<CharT>::CalcStringRectImpl(Rect* pRect, const CharT* str, int length) {
    int remain = length;
    const CharT* pos = str;

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = 0.0f;
    pRect->bottom = 0.0f;

    SetCursor(0.0f, 0.0f);

    do {
        Rect rect;
        int read = CalcLineRectImpl(&rect, pos, remain);

        pos += read;
        remain -= read;

        pRect->left = Min(pRect->left, rect.left);
        pRect->top = Min(pRect->top, rect.top);
        pRect->right = Max(pRect->right, rect.right);
        pRect->bottom = Max(pRect->bottom, rect.bottom);
    } while (remain > 0);
} // 0x803D1350

/*
 * TextWriterBase::PrintImpl
 *
 * Prints `str`, handing control characters to the tag processor; returns the width of the widest line.
 */
template <typename CharT>
f32 TextWriterBase<CharT>::PrintImpl(const CharT* str, int length) {
    f32 xOrigin = GetCursorX();
    f32 yOrigin = GetCursorY();
    f32 orgCursorX = xOrigin;
    f32 orgCursorY = yOrigin;
    // R36: the original sets xCursorAdj and never reads it; retail keeps the GetCursorX call.
    f32 xCursorAdj = 0.0f;
    f32 yCursorAdj = 0.0f;
    f32 textWidth = 0.0f;
    bool bCharSpace = false;

    textWidth = AdjustCursor(&xOrigin, &yOrigin, str, length);
    xCursorAdj = orgCursorX - GetCursorX();
    yCursorAdj = orgCursorY - GetCursorY();

    PrintContext<CharT> context = {this, str, xOrigin, yOrigin, 0};
    CharStrmReader reader = GetFont()->GetCharStrmReader();
    reader.Set(str);

    for (u16 code = reader.Next(); static_cast<const CharT*>(reader.GetCurrentPos()) - str <= length;
         code = reader.Next()) {
        if (code < (char)L' ') {
            context.str = static_cast<const CharT*>(reader.GetCurrentPos());
            context.flags = 0;
            context.flags |= bCharSpace ? 0 : 1 << 0;

            Operation operation = mTagProcessor->Process(code, &context);

            if (operation == OPERATION_NEXT_LINE) {
                if (IsDrawFlagSet(0x003, 0x001)) {
                    int remain = length - (context.str - str);
                    f32 width = CalcLineWidth(context.str, remain);
                    f32 offset = (textWidth - width) / 2.0f;

                    SetCursorX(context.xOrigin + offset);
                } else if (IsDrawFlagSet(0x003, 0x002)) {
                    int remain = length - (context.str - str);
                    f32 width = CalcLineWidth(context.str, remain);
                    f32 offset = textWidth - width;

                    SetCursorX(context.xOrigin + offset);
                } else {
                    f32 width = GetCursorX() - context.xOrigin;

                    textWidth = Max(textWidth, width);
                    SetCursorX(context.xOrigin);
                }

                bCharSpace = false;
            } else if (operation == OPERATION_NO_CHAR_SPACE) {
                bCharSpace = false;
            } else if (operation == OPERATION_CHAR_SPACE) {
                bCharSpace = true;
            } else if (operation == OPERATION_END_DRAW) {
                break;
            }

            reader.Set(context.str);
        } else {
            f32 baseY = GetCursorY();

            if (bCharSpace) {
                MoveCursorX(GetCharSpace());
            }

            bCharSpace = true;

            {
                const Font* pFont = GetFont();
                f32 adj = -pFont->GetBaselinePos() * GetScaleV();

                MoveCursorY(adj);
            }

            CharWriter::Print(code);
            SetCursorY(baseY);
        }
    }

    if (IsDrawFlagSet(0x300, 0x100) || IsDrawFlagSet(0x300, 0x200)) {
        SetCursorY(orgCursorY);
    } else {
        MoveCursorY(yCursorAdj);
    }

    return textWidth;
} // 0x803D1470

/*
 * TextWriterBase::AdjustCursor
 *
 * Moves the cursor to the start of the text according to the alignment flags; returns the text width.
 */
template <typename CharT>
f32 TextWriterBase<CharT>::AdjustCursor(f32* xOrigin, f32* yOrigin, const CharT* str, int length) {
    f32 textWidth = 0.0f;
    f32 textHeight = 0.0f;

    if (!IsDrawFlagSet(0x333, 0x300) && !IsDrawFlagSet(0x333, 0x000)) {
        Rect textRect;

        CalcStringRect(&textRect, str, length);
        textWidth = textRect.left + textRect.right;
        textHeight = textRect.top + textRect.bottom;
    }

    if (IsDrawFlagSet(0x030, 0x010)) {
        *xOrigin -= textWidth / 2.0f;
    } else if (IsDrawFlagSet(0x030, 0x020)) {
        *xOrigin -= textWidth;
    }

    if (IsDrawFlagSet(0x300, 0x100)) {
        *yOrigin -= textHeight / 2.0f;
    } else if (IsDrawFlagSet(0x300, 0x200)) {
        *yOrigin -= textHeight;
    }

    if (IsDrawFlagSet(0x003, 0x001)) {
        f32 width = CalcLineWidth(str, length);
        f32 offset = (textWidth - width) / 2.0f;

        SetCursorX(*xOrigin + offset);
    } else if (IsDrawFlagSet(0x003, 0x002)) {
        f32 width = CalcLineWidth(str, length);
        f32 offset = textWidth - width;

        SetCursorX(*xOrigin + offset);
    } else {
        SetCursorX(*xOrigin);
    }

    if (IsDrawFlagSet(0x300, 0x300)) {
        SetCursorY(*yOrigin);
    } else {
        SetCursorY(*yOrigin + GetFontAscent());
    }

    return textWidth;
} // 0x803D1A20

template class TextWriterBase<char>;
template class TextWriterBase<wchar_t>;

} // namespace ut
} // namespace nw4hbm
