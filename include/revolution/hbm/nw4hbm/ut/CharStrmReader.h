#ifndef NW4HBM_UT_CHAR_STRM_READER_H
#define NW4HBM_UT_CHAR_STRM_READER_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/CharStrmReader.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_CharStrmReader.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {

/*
 * CharStrmReader
 *
 * Reads one character code at a time from a string in a font's encoding
 * (UTF-8, UTF-16, CP1252 or Shift JIS), through the member function chosen at
 * construction.
 */
class CharStrmReader {
public:
    typedef u16 (CharStrmReader::*ReadFunc)();

    CharStrmReader(ReadFunc func) : mCharStrm(NULL), mReadFunc(func) {}
    ~CharStrmReader() {}

    const void* GetCurrentPos() const { return mCharStrm; }

    void Set(const char* stream) { mCharStrm = stream; }
    void Set(const wchar_t* stream) { mCharStrm = stream; }

    u16 ReadNextCharUTF8();
    u16 ReadNextCharUTF16();
    u16 ReadNextCharCP1252();
    u16 ReadNextCharSJIS();

    u16 Next() { return (this->*mReadFunc)(); }

private:
    template <typename CharT>
    CharT GetChar(int offset) const {
        const CharT* charStrm = static_cast<const CharT*>(mCharStrm);
        return charStrm[offset];
    }

    template <typename CharT>
    void StepStrm(int step) {
        const CharT*& charStrm = reinterpret_cast<const CharT*&>(mCharStrm);
        charStrm += step;
    }

    const void* mCharStrm; // 0x00 current read position
    ReadFunc mReadFunc;    // 0x04 decoder for the stream's encoding
}; // size 0x10

} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_CHAR_STRM_READER_H
