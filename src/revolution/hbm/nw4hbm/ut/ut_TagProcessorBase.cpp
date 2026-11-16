// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_TagProcessorBase.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_TagProcessorBase.cpp
#include "nw4hbm/ut/TagProcessorBase.h"

#include "nw4hbm/ut/Rect.h"
#include "nw4hbm/ut/TextWriterBase.h"

namespace nw4hbm {
namespace ut {

/*
 * TagProcessorBase::TagProcessorBase
 */
template <typename CharT>
TagProcessorBase<CharT>::TagProcessorBase() {
} // 0x803CEAB0

/*
 * TagProcessorBase::~TagProcessorBase
 */
template <typename CharT>
TagProcessorBase<CharT>::~TagProcessorBase() {
} // 0x803CEAC0

/*
 * TagProcessorBase::Process
 *
 * Handles a line feed and a tab; every other character is printed as is.
 */
template <typename CharT>
Operation TagProcessorBase<CharT>::Process(u16 code, PrintContext<CharT>* context) {
    switch (code) {
    case L'\n':
        ProcessLinefeed(context);
        return OPERATION_NEXT_LINE;

    case L'\t':
        ProcessTab(context);
        return OPERATION_NO_CHAR_SPACE;

    default:
        return OPERATION_DEFAULT;
    }
} // 0x803CEB00

/*
 * TagProcessorBase::CalcRect
 *
 * As Process, also returning in `pRect` the area the cursor moved across.
 */
template <typename CharT>
Operation TagProcessorBase<CharT>::CalcRect(Rect* pRect, u16 code, PrintContext<CharT>* context) {
    switch (code) {
    case L'\n': {
        TextWriterBase<CharT>& writer = *context->writer;

        pRect->right = writer.GetCursorX();
        pRect->top = writer.GetCursorY();

        ProcessLinefeed(context);

        pRect->left = writer.GetCursorX();
        pRect->bottom = writer.GetCursorY() + context->writer->GetFontHeight();

        pRect->Normalize();
    }
        return OPERATION_NEXT_LINE;

    case L'\t': {
        TextWriterBase<CharT>& writer = *context->writer;

        pRect->left = writer.GetCursorX();

        ProcessTab(context);

        pRect->right = writer.GetCursorX();
        pRect->top = writer.GetCursorY();
        pRect->bottom = pRect->top + writer.GetFontHeight();

        pRect->Normalize();
    }
        return OPERATION_NO_CHAR_SPACE;

    default:
        return OPERATION_DEFAULT;
    }
} // 0x803CEC60

/*
 * TagProcessorBase::ProcessLinefeed
 *
 * Moves the cursor to the start of the next line.
 */
template <typename CharT>
void TagProcessorBase<CharT>::ProcessLinefeed(PrintContext<CharT>* context) {
    TextWriterBase<CharT>& writer = *context->writer;

    f32 x = context->xOrigin;
    f32 y = writer.GetCursorY() + writer.GetLineHeight();

    writer.SetCursor(x, y);
} // 0x803CEEA0

/*
 * TagProcessorBase::ProcessTab
 *
 * Moves the cursor to the next tab stop; tab stops are every GetTabWidth()
 * character widths from the left edge of the text. A tab width of 0 or less
 * leaves the cursor where it is.
 */
template <typename CharT>
void TagProcessorBase<CharT>::ProcessTab(PrintContext<CharT>* context) {
    TextWriterBase<CharT>& writer = *context->writer;
    int tabWidth = writer.GetTabWidth();

    if (tabWidth > 0) {
        f32 aCharWidth = writer.IsWidthFixed() ? writer.GetFixedWidth() : writer.GetFontWidth();
        f32 dx = writer.GetCursorX() - context->xOrigin;
        f32 tabPixel = tabWidth * aCharWidth;
        int numTab = static_cast<int>(dx / tabPixel) + 1;
        f32 x = tabPixel * numTab + context->xOrigin;

        writer.SetCursorX(x);
    }
} // 0x803CEF10

template class TagProcessorBase<char>;
template class TagProcessorBase<wchar_t>;

} // namespace ut
} // namespace nw4hbm
