// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_binaryFileFormat.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_binaryFileFormat.cpp
#include "nw4hbm/ut/binaryFileFormat.h"

namespace nw4hbm {
namespace ut {

/*
 * IsValidBinaryFile
 *
 * Checks a resource file header: kind, byte order, version, and room for at
 * least `minBlocks` data blocks.
 */
bool IsValidBinaryFile(const BinaryFileHeader* header, u32 signature, u16 version, u16 minBlocks) {
    if (header->signature != signature) {
        return false;
    }

    if (header->byteOrder != NW4HBM_BYTEORDER_BIG) {
        return false;
    }

    if (header->version != version) {
        return false;
    }

    if (header->fileSize < sizeof(BinaryFileHeader) + sizeof(BinaryBlockHeader) * minBlocks) {
        return false;
    }

    if (header->dataBlocks < minBlocks) {
        return false;
    }

    return true;
} // 0x803CC290

} // namespace ut
} // namespace nw4hbm
