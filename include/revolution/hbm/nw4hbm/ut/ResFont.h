#ifndef NW4HBM_UT_RES_FONT_H
#define NW4HBM_UT_RES_FONT_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ResFont.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_ResFont.h

#include <revolution/types.h>
#include "nw4hbm/ut/ResFontBase.h"
#include "nw4hbm/ut/binaryFileFormat.h"

namespace nw4hbm {
namespace ut {

/*
 * ResFont
 *
 * Font read from a brfnt file in memory. The first SetResource rebuilds the
 * file in place (block offsets become pointers) and marks it unpacked.
 */
class ResFont : public detail::ResFontBase {
public:
    static const u32 SIGNATURE_FONT = 'RFNT';           // packed file
    static const u32 SIGNATURE_FONT_UNPACKED = 'RFNU';  // file after Rebuild
    static const u32 SIGNATURE_FONT_INFO = 'FINF';      // FontInformation block
    static const u32 SIGNATURE_FONT_TEX_GLYPH = 'TGLP'; // FontTextureGlyph block
    static const u32 SIGNATURE_FONT_CHAR_WIDTH = 'CWDH'; // FontWidth block
    static const u32 SIGNATURE_FONT_CODE_MAP = 'CMAP';  // FontCodeMap block
    static const u32 SIGNATURE_FONT_GLGR = 'GLGR';      // block with nothing to rebuild

    static const u16 FILE_VERSION = 0x0104; // 1.4; 1.2 files are read as well

    ResFont();
    virtual ~ResFont();

    bool SetResource(void* brfnt);
    void* RemoveResource();

    static FontInformation* Rebuild(BinaryFileHeader* fileHeader);
}; // size 0x18

} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_RES_FONT_H
