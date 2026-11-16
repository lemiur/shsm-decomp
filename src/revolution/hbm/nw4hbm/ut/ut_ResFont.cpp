// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_ResFont.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_ResFont.cpp
#include "nw4hbm/ut/ResFont.h"

namespace nw4hbm {
namespace ut {

namespace {

/*
 * ResolveOffset
 *
 * A packed brfnt stores its block links as byte offsets from the start of
 * the file; this turns the offset held in `ptr` into a pointer.
 */
template <typename T>
inline void ResolveOffset(T*& ptr, void* base) {
    *reinterpret_cast<u32*>(&ptr) = reinterpret_cast<u32>(base) + reinterpret_cast<u32>(ptr);
}

/*
 * AddOffset
 *
 * The block header `offset` bytes past `p`: the first block after the file
 * header, or the block after another one.
 */
inline BinaryBlockHeader* AddOffset(void* p, u32 offset) {
    return reinterpret_cast<BinaryBlockHeader*>(reinterpret_cast<u32>(p) + offset);
}

} // namespace

/*
 * ResFont::ResFont
 */
ResFont::ResFont() {
} // 0x803CE160

/*
 * ResFont::~ResFont
 */
ResFont::~ResFont() {
} // 0x803CE1A0

/*
 * ResFont::SetResource
 *
 * Attaches the brfnt file `brfnt`, rebuilding it first unless an earlier
 * call already did. Fails when a resource is attached already or the file
 * is not a valid version 1.4 or 1.2 font.
 */
bool ResFont::SetResource(void* brfnt) {
    FontInformation* pFontInfo = NULL;
    BinaryFileHeader* fileHeader = static_cast<BinaryFileHeader*>(brfnt);

    if (!IsManaging(NULL)) {
        return false;
    }

    if (fileHeader->signature == SIGNATURE_FONT_UNPACKED) {
        BinaryBlockHeader* blockHeader;
        int nBlocks = 0;

        blockHeader = AddOffset(fileHeader, fileHeader->headerSize);

        while (nBlocks < fileHeader->dataBlocks) {
            if (blockHeader->kind == SIGNATURE_FONT_INFO) {
                pFontInfo = reinterpret_cast<FontInformation*>(blockHeader + 1);
                break;
            }

            blockHeader = AddOffset(blockHeader, blockHeader->size);
            ++nBlocks;
        }
    } else {
        if (fileHeader->version == FILE_VERSION) {
            if (!IsValidBinaryFile(fileHeader, SIGNATURE_FONT, FILE_VERSION, 2)) {
                return false;
            }
        } else {
            if (!IsValidBinaryFile(fileHeader, SIGNATURE_FONT, 0x0102, 2)) {
                return false;
            }
        }

        pFontInfo = Rebuild(fileHeader);
    }

    if (!pFontInfo) {
        return false;
    }

    SetResourceBuffer(brfnt, pFontInfo);
    InitReaderFunc(GetEncoding());

    return true;
} // 0x803CE200

/*
 * ResFont::Rebuild
 *
 * Turns the block offsets of the packed brfnt `fileHeader` into pointers
 * and marks the file unpacked. Returns its FINF block, or NULL on an
 * unknown block.
 */
FontInformation* ResFont::Rebuild(BinaryFileHeader* fileHeader) {
    BinaryBlockHeader* blockHeader;
    FontInformation* info = NULL;
    int nBlocks = 0;

    blockHeader = AddOffset(fileHeader, fileHeader->headerSize);

    while (nBlocks < fileHeader->dataBlocks) {
        switch (blockHeader->kind) {
        case SIGNATURE_FONT_INFO: {
            info = reinterpret_cast<FontInformation*>(blockHeader + 1);

            ResolveOffset(info->pGlyph, fileHeader);

            if (info->pWidth) {
                ResolveOffset(info->pWidth, fileHeader);
            }

            if (info->pMap) {
                ResolveOffset(info->pMap, fileHeader);
            }
        } break;

        case SIGNATURE_FONT_TEX_GLYPH: {
            FontTextureGlyph* glyph = reinterpret_cast<FontTextureGlyph*>(blockHeader + 1);

            ResolveOffset(glyph->sheetImage, fileHeader);
        } break;

        case SIGNATURE_FONT_CHAR_WIDTH: {
            FontWidth* width = reinterpret_cast<FontWidth*>(blockHeader + 1);

            if (width->pNext) {
                ResolveOffset(width->pNext, fileHeader);
            }
        } break;

        case SIGNATURE_FONT_CODE_MAP: {
            FontCodeMap* map = reinterpret_cast<FontCodeMap*>(blockHeader + 1);

            if (map->pNext) {
                ResolveOffset(map->pNext, fileHeader);
            }
        } break;

        case SIGNATURE_FONT_GLGR:
            break;

        default:
            return NULL;
        }

        blockHeader = AddOffset(blockHeader, blockHeader->size);
        ++nBlocks;
    }

    fileHeader->signature = SIGNATURE_FONT_UNPACKED;

    return info;
} // 0x803CE350

} // namespace ut
} // namespace nw4hbm
