#ifndef NW4HBM_UT_BINARY_FILE_FORMAT_H
#define NW4HBM_UT_BINARY_FILE_FORMAT_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/binaryFileFormat.h

#include <revolution/types.h>

// Byte-order mark stored in every NW4R binary file header (U+FEFF, big endian).
#define NW4HBM_BYTEORDER_BIG 0xFEFF

namespace nw4hbm {
namespace ut {

/*
 * BinaryFileHeader
 *
 * Header at the start of every NW4R binary resource file (brlyt, brlan, brfnt...).
 */
struct BinaryFileHeader {
    u32 signature;  // 0x00 four-character file kind
    u16 byteOrder;  // 0x04 NW4HBM_BYTEORDER_BIG
    u16 version;    // 0x06
    u32 fileSize;   // 0x08 whole file, header included
    u16 headerSize; // 0x0C
    u16 dataBlocks; // 0x0E number of BinaryBlockHeader blocks that follow
}; // size 0x10

/*
 * BinaryBlockHeader
 *
 * Header of each data block that follows the BinaryFileHeader.
 */
struct BinaryBlockHeader {
    u32 kind; // 0x00 four-character block kind
    u32 size; // 0x04 whole block, header included
}; // size 0x08

bool IsValidBinaryFile(const BinaryFileHeader* header, u32 signature, u16 version, u16 minBlocks);

} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_BINARY_FILE_FORMAT_H
