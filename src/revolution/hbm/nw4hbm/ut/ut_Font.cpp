// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_Font.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_Font.cpp
#include "nw4hbm/ut/Font.h"

namespace nw4hbm {
namespace ut {

/*
 * Font::InitReaderFunc
 *
 * Selects the CharStrmReader decoder for `encoding`; an unknown encoding
 * reads as CP1252.
 */
void Font::InitReaderFunc(FontEncoding encoding) {
    switch (encoding) {
    case FONT_ENCODING_UTF8:
        mReaderFunc = &CharStrmReader::ReadNextCharUTF8;
        break;

    case FONT_ENCODING_UTF16:
        mReaderFunc = &CharStrmReader::ReadNextCharUTF16;
        break;

    case FONT_ENCODING_SJIS:
        mReaderFunc = &CharStrmReader::ReadNextCharSJIS;
        break;

    case FONT_ENCODING_CP1252:
    default:
        mReaderFunc = &CharStrmReader::ReadNextCharCP1252;
        break;
    }
} // 0x803CDE30

} // namespace ut
} // namespace nw4hbm
