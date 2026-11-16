#ifndef NW4HBM_LYT_COMMON_H
#define NW4HBM_LYT_COMMON_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/common.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_common.h

#include <revolution/types.h>
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"

#include <revolution/tpl.h>

namespace nw4hbm {
namespace lyt {

namespace res {

/*
 * BinaryFileHeader
 *
 * Header at the start of a layout or animation file (brlyt, brlan).
 */
struct BinaryFileHeader {
    char signature[4]; // 0x00 four-character file kind
    u16 byteOrder;     // 0x04 byte-order mark, 0xFEFF (big endian)
    u16 version;       // 0x06
    u32 fileSize;      // 0x08 whole file, header included
    u16 headerSize;    // 0x0C
    u16 dataBlocks;    // 0x0E number of data blocks that follow
}; // size 0x10

/*
 * DataBlockHeader
 *
 * Header of each data block of a layout file.
 */
struct DataBlockHeader {
    char kind[4]; // 0x00 four-character block kind
    u32 size;     // 0x04 whole block, header included
}; // size 0x08

} // namespace res

/*
 * Size
 *
 * Width and height of a pane.
 */
struct Size {
    Size() : width(0.0f), height(0.0f) {}
    Size(f32 aWidth, f32 aHeight) : width(aWidth), height(aHeight) {}
    Size(const Size& other) : width(other.width), height(other.height) {}

    friend bool operator==(const Size& a, const Size& b) { return a.width == b.width && a.height == b.height; }

    f32 width;  // 0x00
    f32 height; // 0x04
}; // size 0x08

namespace detail {

typedef math::VEC2 TexCoords[4];

/*
 * TexCoordAry
 *
 * The texture coordinates of a pane: `mNum` sets of four corner coordinates
 * held in storage reserved for `mCap` sets.
 */
class TexCoordAry {
public:
    TexCoordAry();

    u8 GetSize() const { return mNum; }
    const TexCoords* GetArray() const { return mpData; }
    bool IsEmpty() const { return mCap == 0; }

    void SetSize(u8 num);
    void GetCoord(u32 idx, math::VEC2* coord) const;
    void SetCoord(u32 idx, const math::VEC2* coord);
    void Reserve(u8 num);
    void Free();
    void Copy(const void* pResTexCoord, u8 texCoordNum);

private:
    u8 mCap;           // 0x00
    u8 mNum;           // 0x01
    TexCoords* mpData; // 0x04
}; // size 0x08

/*
 * GetSignatureInt
 *
 * Reads a four-character signature as one word.
 */
inline s32 GetSignatureInt(const char* sig) {
    return *reinterpret_cast<const s32*>(sig);
}

/*
 * GetVtxColorElement
 *
 * Reads one colour byte: element `idx` counts the bytes of the four vertex colours in order.
 */
inline u8 GetVtxColorElement(const ut::Color* cols, u32 idx) {
    return reinterpret_cast<const u8*>(cols + idx / 4)[idx % 4];
}

/*
 * SetVtxColorElement
 *
 * Writes one colour byte, numbered as in GetVtxColorElement.
 */
inline void SetVtxColorElement(ut::Color* cols, u32 idx, u8 value) {
    reinterpret_cast<u8*>(cols + idx / 4)[idx % 4] = value;
}

/*
 * GetHorizontalPosition
 *
 * A text or pane position packs horizontal + 3 * vertical (0 left/top, 1 centre, 2 right/bottom).
 */
inline u8 GetHorizontalPosition(u8 var) {
    return var % 3;
}

/*
 * GetVerticalPosition
 */
inline u8 GetVerticalPosition(u8 var) {
    return var / 3;
}

/*
 * SetHorizontalPosition
 */
inline void SetHorizontalPosition(u8* pVar, u8 newVal) {
    *pVar = GetVerticalPosition(*pVar) * 3 + newVal;
}

/*
 * SetVerticalPosition
 */
inline void SetVerticalPosition(u8* pVar, u8 newVal) {
    *pVar = newVal * 3 + GetHorizontalPosition(*pVar);
}

/*
 * GetStrTableStr
 *
 * Returns string `index` of a string table: an array of offsets, measured from
 * the table's start, followed by the strings.
 */
inline const char* GetStrTableStr(const void* pStrTable, int index) {
    const u32* offsets = static_cast<const u32*>(pStrTable);
    const char* stringPool = static_cast<const char*>(pStrTable);

    return stringPool + offsets[index];
}

bool EqualsPaneName(const char* name1, const char* name2);
bool EqualsPaneName(const u8* name1, const u8* name2);

bool EqualsMaterialName(const char* name1, const char* name2);

bool TestFileHeader(const res::BinaryFileHeader& fileHeader);
bool TestFileHeader(const res::BinaryFileHeader& fileHeader, u32 testSig);

bool IsModulateVertexColor(ut::Color* vtxColors, u8 glbAlpha);
ut::Color MultipleAlpha(const ut::Color col, u8 alpha);
void MultipleAlpha(ut::Color* dst, const ut::Color* src, u8 alpha);

void SetVertexFormat(bool bModulate, u8 texCoordNum);

void DrawQuad(const math::VEC2& polLT, const math::VEC2& polRB, const math::VEC2& texLT, const math::VEC2& texRB,
              const ut::Color* vtxColors);
void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors);
void DrawQuad(const math::VEC2& basePt, const Size& size, u8 texCoordNum, const TexCoords* texCoords,
              const ut::Color* vtxColors, u8 alpha);
void DrawLine(const math::VEC2& pos, const Size& size, ut::Color color);
void InitGXTexObjFromTPL(GXTexObj* to, TPLPalette* pal, u32 id);

} // namespace detail

} // namespace lyt
} // namespace nw4hbm

#endif
