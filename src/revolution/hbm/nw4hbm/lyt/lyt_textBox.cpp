// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_textBox.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_textBox.cpp
#include "nw4hbm/lyt/textBox.h"

#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/drawInfo.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/material.h"
#include "nw4hbm/lyt/resourceAccessor.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/CharStrmReader.h"
#include "nw4hbm/ut/CharWriter.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/Font.h"
#include "nw4hbm/ut/inlines.h"
#include "nw4hbm/ut/Rect.h"
#include "nw4hbm/ut/ResFont.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"
#include "nw4hbm/ut/TagProcessorBase.h"
#include "nw4hbm/ut/TextWriterBase.h"

#include <new>
#include <string.h>
#include <wchar.h>

namespace {

using namespace nw4hbm;
using namespace nw4hbm::lyt;

ut::Color GetColor(const GXColorS10& src);
u8 ClampColor(s16 colVal);

template <typename CharT>
int CalcLineStrNum(f32* pWidth, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                   f32 maxWidth, bool* pbOver);

template <typename CharT>
int CalcLineRectImpl(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                     f32 maxWidth, bool* pbOver);

template <typename CharT>
void CalcStringRect(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                    f32 maxWidth);

template <typename CharT>
void CalcStringRectImpl(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                        f32 maxWidth);

} // namespace

namespace nw4hbm {
namespace lyt {

const ut::detail::RuntimeTypeInfo TextBox::typeInfo(&Pane::typeInfo);

} // namespace lyt
} // namespace nw4hbm

namespace {

/*
 * GetColor
 *
 * Clamps a signed TEV colour to a byte colour.
 */
ut::Color GetColor(const GXColorS10& src) {
    GXColor dst;

    dst.r = ClampColor(src.r);
    dst.g = ClampColor(src.g);
    dst.b = ClampColor(src.b);
    dst.a = ClampColor(src.a);

    return dst;
} // no copy in retail (inlined)

/*
 * ClampColor
 */
u8 ClampColor(s16 colVal) {
    return colVal < 0 ? 0 : colVal > 0xFF ? 0xFF : colVal;
} // no copy in retail (inlined)

/*
 * CalcLineStrNum
 *
 * Measures one line on a copy of the writer, so that the caller's cursor stays put: the number
 * of characters that fit in `maxWidth`, and their width in `*pWidth`.
 */
template <typename CharT>
int CalcLineStrNum(f32* pWidth, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                   f32 maxWidth, bool* pbOver) {
    ut::Rect rect;
    ut::TextWriterBase<CharT> myCopy = *pTextWriter;
    myCopy.SetCursor(0.0f, 0.0f);

    int ret = CalcLineRectImpl(&rect, &myCopy, str, length, maxWidth, pbOver);

    *pWidth = rect.GetWidth();
    return ret;
} // no copy in retail (inlined)

/*
 * CalcLineRectImpl
 *
 * Lays out one line of `str` (at most `length` characters) and returns the number of
 * characters read. A line that would grow wider than `maxWidth` stops before the character that
 * overflows it, with `*pbOver` set; control characters go through the writer's tag processor.
 * Only the wchar_t instance exists (TextBox's strings are UTF-16).
 */
template <typename CharT>
int CalcLineRectImpl(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                     f32 maxWidth, bool* pbOver) {
    ut::PrintContext<CharT> context = {pTextWriter, str, 0.0f, 0.0f, 0};
    const ut::Font* font = pTextWriter->GetFont();

    f32 x = 0.0f;
    bool bCharSpace = false;

    ut::CharStrmReader reader = font->GetCharStrmReader();
    const CharT* prStrPos = static_cast<const CharT*>(reader.GetCurrentPos());

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = ut::Min(0.0f, pTextWriter->GetLineHeight());
    pRect->bottom = ut::Max(0.0f, pTextWriter->GetLineHeight());

    *pbOver = false;
    reader.Set(str);

    ut::Rect prMaxRect = *pRect;

    for (u16 code = reader.Next(); static_cast<const CharT*>(reader.GetCurrentPos()) - str <= length;
         prStrPos = static_cast<const CharT*>(reader.GetCurrentPos()), code = reader.Next(), prMaxRect = *pRect) {
        if ((int)code < L' ') {
            ut::Operation operation;
            ut::Rect rect(x, 0.0f, 0.0f, 0.0f);

            context.str = static_cast<const CharT*>(reader.GetCurrentPos());
            context.flags = 0;
            context.flags |= (bCharSpace ? 0 : 1) << 0;

            pTextWriter->SetCursorX(x);

            operation = pTextWriter->GetTagProcessor().CalcRect(&rect, code, &context);

            reader.Set(context.str);

            pRect->left = ut::Min(pRect->left, rect.left);
            pRect->top = ut::Min(pRect->top, rect.top);
            pRect->right = ut::Max(pRect->right, rect.right);
            pRect->bottom = ut::Max(pRect->bottom, rect.bottom);

            x = pTextWriter->GetCursorX();

            if (pRect->GetWidth() > maxWidth) {
                *pbOver = true;
                break;
            }

            if (operation == ut::OPERATION_END_DRAW) {
                return length;
            } else if (operation == ut::OPERATION_NO_CHAR_SPACE) {
                bCharSpace = false;
            } else if (operation == ut::OPERATION_CHAR_SPACE) {
                bCharSpace = true;
            } else if (operation == ut::OPERATION_NEXT_LINE) {
                break;
            }
        } else {
            if (bCharSpace) {
                x += pTextWriter->GetCharSpace();
            }

            bCharSpace = true;

            if (pTextWriter->IsWidthFixed()) {
                x += pTextWriter->GetFixedWidth();
            } else {
                x += pTextWriter->GetFont()->GetCharWidth(code) * pTextWriter->GetScaleH();
            }

            pRect->left = ut::Min(pRect->left, x);
            pRect->right = ut::Max(pRect->right, x);

            if (pRect->GetWidth() > maxWidth) {
                *pbOver = true;
                break;
            }
        }
    }

    if (*pbOver && prStrPos) {
        *pRect = prMaxRect;

        return prStrPos - str;
    } else {
        return static_cast<const CharT*>(reader.GetCurrentPos()) - str;
    }
} // 0x803C92A0

/*
 * CalcStringRect
 *
 * Measures the whole string on a copy of the writer.
 */
template <typename CharT>
void CalcStringRect(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                    f32 maxWidth) {
    ut::TextWriterBase<CharT> myCopy = *pTextWriter;

    CalcStringRectImpl(pRect, &myCopy, str, length, maxWidth);
} // no copy in retail (inlined)

/*
 * CalcStringRectImpl
 *
 * The bounding rectangle of the string, wrapped into lines no wider than `maxWidth`. Only the
 * wchar_t instance exists.
 */
template <typename CharT>
void CalcStringRectImpl(ut::Rect* pRect, ut::TextWriterBase<CharT>* pTextWriter, const CharT* str, int length,
                        f32 maxWidth) {
    int remain = length;
    const CharT* pos = str;

    pRect->left = 0.0f;
    pRect->right = 0.0f;
    pRect->top = 0.0f;
    pRect->bottom = 0.0f;

    pTextWriter->SetCursor(0.0f, 0.0f);

    do {
        ut::Rect rect;
        bool bOver;
        int read = CalcLineRectImpl(&rect, pTextWriter, pos, remain, maxWidth, &bOver);

        if (bOver) {
            CalcLineRectImpl(&rect, pTextWriter, L"\n", 1, maxWidth, &bOver);
        }

        pos += read;
        remain -= read;

        pRect->left = ut::Min(pRect->left, rect.left);
        pRect->top = ut::Min(pRect->top, rect.top);
        pRect->right = ut::Max(pRect->right, rect.right);
        pRect->bottom = ut::Max(pRect->bottom, rect.bottom);
    } while (remain > 0);
} // 0x803C9940

} // namespace

namespace nw4hbm {
namespace lyt {

/*
 * TextBox::TextBox
 *
 * Builds the text box from its data block: string, colours, font metrics, the font (looked up
 * by name, or loaded from a font resource and owned by the text box) and the material.
 */
TextBox::TextBox(const res::TextBox* pBlock, const ResBlockSet& resBlockSet) : Pane(pBlock) {
    u16 allocStrBufLen = pBlock->textBufBytes / sizeof(wchar_t);
    if (allocStrBufLen != 0) {
        allocStrBufLen -= 1;
    }

    Init(allocStrBufLen);

    if (pBlock->textStrBytes >= sizeof(wchar_t) && mTextBuf) {
        const wchar_t* pBlockText = detail::ConvertOffsToPtr<wchar_t>(pBlock, pBlock->textStrOffset);
        const u16 resStrLen = pBlock->textStrBytes / sizeof(wchar_t) - 1;

        SetString(pBlockText, 0, resStrLen);
    }

    for (int i = 0; i < (int)(sizeof mTextColors / sizeof mTextColors[0]); ++i) {
        mTextColors[i] = pBlock->textCols[i];
    }

    mFontSize = pBlock->fontSize;
    mTextPosition = pBlock->textPosition;
    mCharSpace = pBlock->charSpace;
    mLineSpace = pBlock->lineSpace;

    const res::Font* fonts = detail::ConvertOffsToPtr<res::Font>(resBlockSet.pFontList, sizeof *resBlockSet.pFontList);
    const char* fontName = detail::ConvertOffsToPtr<char>(fonts, fonts[pBlock->fontIdx].nameStrOffset);

    if (const ut::Font* pFont = resBlockSet.pResAccessor->GetFont(fontName)) {
        mpFont = pFont;
    } else if (void* fontRes = resBlockSet.pResAccessor->GetResource(ResourceAccessor::SIGNATURE_FONT, fontName, NULL)) {
        if (void* pMemFont = Layout::AllocMemory(sizeof(ut::ResFont))) {
            ut::ResFont* pResFont = new (pMemFont) ut::ResFont;
            pResFont->SetResource(fontRes);

            mpFont = pResFont;
            mTextBoxFlag.allocFont = true;
        }
    }

    if (void* pMemMaterial = Layout::AllocMemory(sizeof(Material))) {
        const u32* matOffsTbl =
            detail::ConvertOffsToPtr<u32>(resBlockSet.pMaterialList, sizeof *resBlockSet.pMaterialList);
        const res::Material* pResMaterial =
            detail::ConvertOffsToPtr<res::Material>(resBlockSet.pMaterialList, matOffsTbl[pBlock->materialIdx]);

        mpMaterial = new (pMemMaterial) Material(pResMaterial, resBlockSet);
    }
} // 0x803C8690

/*
 * TextBox::Init
 */
void TextBox::Init(u16 allocStrLen) {
    mTextBuf = NULL;
    mTextBufBytes = 0;
    mTextLen = 0;

    mpFont = NULL;
    mFontSize = Size(0.0f, 0.0f);

    SetTextPositionH(1);
    SetTextPositionV(1);

    mLineSpace = 0.0f;
    mCharSpace = 0.0f;
    mpTagProcessor = NULL;

    memset(&mTextBoxFlag, 0, sizeof mTextBoxFlag);

    if (allocStrLen != 0) {
        AllocStringBuffer(allocStrLen);
    }
} // no copy in retail (inlined)

/*
 * TextBox::~TextBox
 *
 * SetFont(NULL) is expanded here with its font-size branch, which NULL never takes. Built
 * without small data, the compiler addresses that dead branch's int-to-float constant with a
 * low-half relocation off r3, which by then holds GetHeight's result: taken, the branch would
 * load garbage (R36: the compiler's slip, in code that never runs).
 */
TextBox::~TextBox() {
    SetFont(NULL);

    if (mpMaterial && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
        mpMaterial = NULL;
    }

    FreeStringBuffer();
} // 0x803C8950

/*
 * TextBox::GetVtxColor
 *
 * The four corners share the two text colours: the top two take the top colour.
 */
ut::Color TextBox::GetVtxColor(u32 idx) const {
    return GetTextColor(idx / 2);
} // 0x803C8AE0

/*
 * TextBox::SetVtxColor
 */
void TextBox::SetVtxColor(u32 idx, ut::Color value) {
    SetTextColor(idx / 2, value);
} // 0x803C8B00

/*
 * TextBox::GetVtxColorElement
 */
u8 TextBox::GetVtxColorElement(u32 idx) const {
    return reinterpret_cast<const u8*>(mTextColors + idx / 8)[idx % 4];
} // 0x803C8B40

/*
 * TextBox::SetVtxColorElement
 */
void TextBox::SetVtxColorElement(u32 idx, u8 value) {
    reinterpret_cast<u8*>(mTextColors + idx / 8)[idx % 4] = value;
} // 0x803C8B60

/*
 * TextBox::DrawSelf
 *
 * Prints the string line by line, each line placed horizontally by the text position.
 */
void TextBox::DrawSelf(const DrawInfo& drawInfo) {
    if (!mTextBuf || !mpFont || !mpMaterial) {
        return;
    }

    LoadMtx(drawInfo);

    ut::TextWriterBase<wchar_t> writer;
    writer.SetFont(*mpFont);
    writer.SetFontSize(mFontSize.width, mFontSize.height);
    writer.SetLineSpace(mLineSpace);
    writer.SetCharSpace(mCharSpace);

    ut::Color topCol = detail::MultipleAlpha(mTextColors[0], mGlbAlpha);
    ut::Color btmCol = detail::MultipleAlpha(mTextColors[1], mGlbAlpha);
    writer.SetGradationMode(topCol != btmCol ? ut::CharWriter::GRADMODE_V : ut::CharWriter::GRADMODE_NONE);
    writer.SetTextColor(topCol, btmCol);

    ut::Color minCol = GetColor(mpMaterial->GetTevColor(0));
    ut::Color maxCol = GetColor(mpMaterial->GetTevColor(1));
    writer.SetColorMapping(minCol, maxCol);

    if (mpTagProcessor) {
        writer.SetTagProcessor(mpTagProcessor);
    }

    writer.SetupGX();

    ut::Rect textRect = GetTextDrawRect(&writer);

    f32 hMag = GetTextMagH();
    wchar_t* strPos = mTextBuf;
    f32 textWidth = textRect.GetWidth();

    writer.SetCursor(textRect.left, textRect.top);

    for (int remain = mTextLen; remain > 0;) {
        f32 lineWidth;
        bool bOver;
        int lineStrNum = CalcLineStrNum(&lineWidth, &writer, strPos, remain, mSize.width, &bOver);
        f32 textPosX = hMag * (textWidth - lineWidth);

        writer.SetCursorX(textRect.left + textPosX);
        writer.Print(strPos, lineStrNum);

        if (bOver) {
            writer.Print(L"\n");
        }

        strPos += lineStrNum;
        remain -= lineStrNum;
    }
} // 0x803C8B80

/*
 * TextBox::GetStringBufferLength
 *
 * Capacity in characters, without the terminator.
 */
u16 TextBox::GetStringBufferLength() const {
    if (mTextBufBytes == 0) {
        return 0;
    }

    return mTextBufBytes / sizeof(wchar_t) - 1;
} // no copy in retail (inlined)

/*
 * TextBox::AllocStringBuffer
 *
 * Grows the string buffer to hold at least `minLen` characters and the terminator; a buffer
 * that is large enough already is kept.
 */
void TextBox::AllocStringBuffer(u16 minLen) {
    if (minLen == 0) {
        return;
    }

    u16 allocBytes = sizeof(wchar_t) * (minLen + 1);
    if (allocBytes <= mTextBufBytes) {
        return;
    }

    FreeStringBuffer();

    mTextBuf = static_cast<wchar_t*>(Layout::AllocMemory(allocBytes));
    if (mTextBuf) {
        mTextBufBytes = allocBytes;
    }
} // 0x803C96F0

/*
 * TextBox::FreeStringBuffer
 */
void TextBox::FreeStringBuffer() {
    if (mTextBuf) {
        Layout::FreeMemory(mTextBuf);
        mTextBuf = NULL;
        mTextBufBytes = 0;
    }
} // 0x803C9770

/*
 * TextBox::SetString
 */
u16 TextBox::SetString(const wchar_t* str, u16 dstIdx) {
    return SetString(str, dstIdx, wcslen(str));
} // 0x803C97C0

/*
 * TextBox::SetString
 *
 * Copies up to `strLen` characters to the buffer from `dstIdx` on, as many as fit, and ends the
 * string after them. Returns the number copied.
 */
u16 TextBox::SetString(const wchar_t* str, u16 dstIdx, u16 strLen) {
    if (!mTextBuf) {
        return 0;
    }

    u16 bufLen = GetStringBufferLength();
    if (dstIdx >= bufLen) {
        return 0;
    }

    const u16 cpLen = ut::Min<u16>(strLen, bufLen - dstIdx);
    memcpy(&mTextBuf[dstIdx], str, sizeof(wchar_t) * cpLen);

    mTextLen = dstIdx + cpLen;
    mTextBuf[mTextLen] = L'\0';

    return cpLen;
} // 0x803C9830

/*
 * TextBox::SetFont
 *
 * Releases a font the text box allocated itself, then takes the new font and its size.
 */
void TextBox::SetFont(const ut::Font* pFont) {
    if (mTextBoxFlag.allocFont) {
        mpFont->~Font();
        Layout::FreeMemory(const_cast<ut::Font*>(mpFont));
        mTextBoxFlag.allocFont = false;
    }

    mpFont = pFont;

    if (mpFont) {
        SetFontSize(Size(mpFont->GetWidth(), mpFont->GetHeight()));
    } else {
        SetFontSize(Size(0.0f, 0.0f));
    }
} // no copy in retail (inlined)

/*
 * TextBox::GetTextDrawRect
 *
 * The rectangle the string occupies, placed inside the pane by the text position.
 */
ut::Rect TextBox::GetTextDrawRect(ut::TextWriterBase<wchar_t>* pWriter) const {
    ut::Rect textRect;

    pWriter->SetCursor(0.0f, 0.0f);
    CalcStringRect(&textRect, pWriter, mTextBuf, mTextLen, mSize.width);

    math::VEC2 basePt = GetVtxPos();

    textRect.MoveTo(basePt.x + (mSize.width - textRect.GetWidth()) * GetTextMagH(),
                    basePt.y + (mSize.height - textRect.GetHeight()) * GetTextMagV());

    return textRect;
} // no copy in retail (inlined)

/*
 * TextBox::GetTextMagH
 *
 * The horizontal text position as a fraction of the free width: 0 left, 0.5 centre, 1 right.
 */
f32 TextBox::GetTextMagH() const {
    f32 hMag = 0.0f;

    switch (GetTextPositionH()) {
    default:
    case 0:
        hMag = 0.0f;
        break;

    case 1:
        hMag = 0.5f;
        break;

    case 2:
        hMag = 1.0f;
        break;
    }

    return hMag;
} // no copy in retail (inlined)

/*
 * TextBox::GetTextMagV
 *
 * The vertical text position as a fraction of the free height: 0 top, 0.5 centre, 1 bottom.
 */
f32 TextBox::GetTextMagV() const {
    f32 vMag = 0.0f;

    switch (GetTextPositionV()) {
    default:
    case 0:
        vMag = 0.0f;
        break;

    case 1:
        vMag = 0.5f;
        break;

    case 2:
        vMag = 1.0f;
        break;
    }

    return vMag;
} // no copy in retail (inlined)

} // namespace lyt
} // namespace nw4hbm
