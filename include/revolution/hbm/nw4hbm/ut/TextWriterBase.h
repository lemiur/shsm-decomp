#ifndef NW4HBM_UT_TEXT_WRITER_BASE_H
#define NW4HBM_UT_TEXT_WRITER_BASE_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/TextWriterBase.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_TextWriterBase.h

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <revolution/types.h>
#include "nw4hbm/ut/CharWriter.h"

namespace nw4hbm {
namespace ut {

struct Rect;
template <typename CharT> class TagProcessorBase;

/*
 * TextWriterBase
 *
 * Prints strings of CharT with a CharWriter: line and character spacing,
 * tab stops, alignment flags and a tag processor for control characters.
 */
template <typename CharT>
class TextWriterBase : public CharWriter {
public:
    TextWriterBase();
    ~TextWriterBase();

    void SetLineHeight(f32 lineHeight);
    f32 GetLineHeight() const;
    void SetLineSpace(f32 lineSpace);
    void SetCharSpace(f32 charSpace);
    f32 GetLineSpace() const;
    f32 GetCharSpace() const;
    void SetTabWidth(int tabWidth);
    int GetTabWidth() const;
    void SetDrawFlag(u32 flags);
    u32 GetDrawFlag() const;
    void SetTagProcessor(TagProcessorBase<CharT>* tagProcessor);
    void ResetTagProcessor();
    TagProcessorBase<CharT>& GetTagProcessor() const;
    f32 CalcFormatStringWidth(const CharT* format, ...) const;
    f32 CalcFormatStringHeight(const CharT* format, ...) const;
    void CalcFormatStringRect(Rect* pRect, const CharT* format, ...) const;
    void CalcVStringRect(Rect* pRect, const CharT* format, va_list args) const;
    f32 CalcStringWidth(const CharT* str, int length) const;
    f32 CalcStringWidth(const CharT* str) const;
    f32 CalcStringHeight(const CharT* str, int length) const;
    f32 CalcStringHeight(const CharT* str) const;
    void CalcStringRect(Rect* pRect, const CharT* str, int length) const;
    void CalcStringRect(Rect* pRect, const CharT* str) const;
    f32 Printf(const CharT* format, ...);
    f32 VPrintf(const CharT* format, va_list args);
    f32 Print(const CharT* str, int length);
    f32 Print(const CharT* str);
    static CharT* SetBuffer(CharT* buf, u32 size);
    static CharT* SetBuffer(u32 size);
    static u32 GetBufferSize();
    static CharT* GetBuffer();

    static int VSNPrintf(char* buffer, u32 count, const char* format, va_list arg) {
        return vsnprintf(buffer, count, format, arg);
    }

    static int VSNPrintf(wchar_t* buffer, u32 count, const wchar_t* format, va_list arg) {
        return vswprintf(buffer, count, format, arg);
    }

    static int StrLen(const char* str) { return strlen(str); }
    static int StrLen(const wchar_t* str) { return wcslen(str); }

    f32 CalcLineWidth(const CharT* str, int length);
    int CalcLineRectImpl(Rect* pRect, const CharT* str, int length);
    void CalcStringRectImpl(Rect* pRect, const CharT* str, int length);
    f32 PrintImpl(const CharT* str, int length);
    f32 AdjustCursor(f32* xOrigin, f32* yOrigin, const CharT* str, int length);

    bool IsDrawFlagSet(u32 mask, u32 flag) const { return (mDrawFlag & mask) == flag; }

private:
    static CharT* mFormatBuffer;
    static u32 mFormatBufferSize;
    static TagProcessorBase<CharT> mDefaultTagProcessor;

    f32 mCharSpace;                         // 0x4C
    f32 mLineSpace;                         // 0x50
    int mTabWidth;                          // 0x54
    u32 mDrawFlag;                          // 0x58
    TagProcessorBase<CharT>* mTagProcessor; // 0x5C
};

} // namespace ut
} // namespace nw4hbm

#endif
