// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_CharStrmReader.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_CharStrmReader.cpp
#include "nw4hbm/ut/CharStrmReader.h"

namespace nw4hbm {
namespace ut {
namespace {

// First byte of a two-byte Shift JIS character.
inline bool IsSJISLeadByte(u8 c) {
    return (0x81 <= c && c < 0xA0) || 0xE0 <= c;
}

} // namespace

/*
 * CharStrmReader::ReadNextCharUTF8
 *
 * Decodes one UTF-8 sequence of one to three bytes.
 */
u16 CharStrmReader::ReadNextCharUTF8() {
    u16 code;

    if ((GetChar<u8>(0) & 0x80) == 0x00) {
        code = GetChar<u8>(0);
        StepStrm<u8>(1);
    } else if ((GetChar<u8>(0) & 0xE0) == 0xC0) {
        code = (GetChar<u8>(0) & 0x1F) << 6 | (GetChar<u8>(1) & 0x3F);
        StepStrm<u8>(2);
    } else {
        // Original quirk: a three-byte lead byte carries four payload bits, but the
        // mask keeps five (the extra bit is shifted out of the 16-bit code).
        code = (GetChar<u8>(0) & 0x1F) << 12 | (GetChar<u8>(1) & 0x3F) << 6 | (GetChar<u8>(2) & 0x3F);
        StepStrm<u8>(3);
    }

    return code;
} // 0x803CC310

/*
 * CharStrmReader::ReadNextCharUTF16
 *
 * Reads one UTF-16 code unit.
 */
u16 CharStrmReader::ReadNextCharUTF16() {
    u16 code = GetChar<u16>(0);
    StepStrm<u16>(1);
    return code;
} // 0x803CC390

/*
 * CharStrmReader::ReadNextCharCP1252
 *
 * Reads one single-byte character.
 */
u16 CharStrmReader::ReadNextCharCP1252() {
    u16 code = GetChar<u8>(0);
    StepStrm<u8>(1);
    return code;
} // 0x803CC3B0

/*
 * CharStrmReader::ReadNextCharSJIS
 *
 * Reads one Shift JIS character of one or two bytes.
 */
u16 CharStrmReader::ReadNextCharSJIS() {
    u16 code;

    if (IsSJISLeadByte(GetChar<u8>(0))) {
        code = GetChar<u8>(0) << 8 | GetChar<u8>(1);
        StepStrm<u8>(2);
    } else {
        code = GetChar<u8>(0);
        StepStrm<u8>(1);
    }

    return code;
} // 0x803CC3D0

} // namespace ut
} // namespace nw4hbm
