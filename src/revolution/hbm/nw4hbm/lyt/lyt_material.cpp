// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_material.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_material.cpp
#include "nw4hbm/lyt/material.h"

#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/resourceAccessor.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/arithmetic.h"
#include "nw4hbm/math/triangular.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/LinkList.h"
#include "nw4hbm/ut/inlines.h"

#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <revolution/tpl.h>

#include <string.h>

namespace {

using namespace nw4hbm;
using namespace nw4hbm::lyt;

bool operator==(const GXColorS10& a, const GXColorS10& b);
bool operator!=(const GXColorS10& a, const GXColorS10& b);

void CalcTextureMtx(math::MTX34* pMtx, const TexSRT& texSRT);
void CalcIndTexMtx(f32 mtx[2][3], const TexSRT& texSRT);

void SetColorComponentValue(ut::Color* pCol, u32 compIdx, s16 value);
void SetIndTexMtx(GXIndTexMtxID id, const f32 mtx[2][3]);

void InitTexSRT(TexSRT* texSRTs, u32 num);

u32 CalcOffsetTexMapAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetTexSRTAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetTexCoordGenAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetChanCtrlAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetMatColAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetTevSwapAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetGetAlphaCompare(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetBlendMode(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetIndirectStageAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetIndTexSRTAry(const detail::BitGXNums& bitGXNums);
u32 CalcOffsetTevStageAry(const detail::BitGXNums& bitGXNums);

void CopyGXTexObj(GXTexObj* pDst, const GXTexObj* pSrc);

/*
 * GetTexMtx
 *
 * Texture matrices occupy every third matrix slot from GX_TEXMTX0.
 */
inline u32 GetTexMtx(u32 texMtxIdx) {
    return texMtxIdx * 3 + GX_TEXMTX0;
}

/*
 * GetTexMtxIdx
 *
 * The index of the texture matrix in matrix slot `texMtx`.
 */
inline u32 GetTexMtxIdx(u32 texMtx) {
    return (texMtx - GX_TEXMTX0) / 3;
}

} // namespace

// .rodata
static const GXColorS10 DefaultBlackColor = {0x0000, 0x0000, 0x0000, 0x0000};
static const GXColorS10 DefaultWhiteColor = {0x00FF, 0x00FF, 0x00FF, 0x00FF};

namespace {

/*
 * operator==
 */
bool operator==(const GXColorS10& a, const GXColorS10& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
} // no copy in retail (inlined)

/*
 * operator!=
 */
bool operator!=(const GXColorS10& a, const GXColorS10& b) {
    return !operator==(a, b);
} // no copy in retail (inlined)

/*
 * CalcTextureMtx
 *
 * Builds the texture matrix of a scale/rotate/translate set; the rotation and scaling
 * are about the middle of the texture.
 */
void CalcTextureMtx(math::MTX34* pMtx, const TexSRT& texSRT) {
    math::VEC2 center(0.5f, 0.5f);

    f32 cosR = math::CosDeg(texSRT.rotate);
    f32 sinR = math::SinDeg(texSRT.rotate);

    f32 a0 = cosR * texSRT.scale.x;
    f32 a1 = -sinR * texSRT.scale.y;
    pMtx->_00 = a0;
    pMtx->_01 = a1;
    pMtx->_02 = 0.0f;
    pMtx->_03 = texSRT.translate.x + center.x + a0 * -center.x + a1 * -center.y;

    a0 = sinR * texSRT.scale.x;
    a1 = cosR * texSRT.scale.y;
    pMtx->_10 = a0;
    pMtx->_11 = a1;
    pMtx->_12 = 0.0f;
    pMtx->_13 = texSRT.translate.y + center.y + a0 * -center.x + a1 * -center.y;

    pMtx->_20 = 0.0f;
    pMtx->_21 = 0.0f;
    pMtx->_22 = 1.0f;
    pMtx->_23 = 0.0f;
} // 0x803C3E70

/*
 * CalcIndTexMtx
 *
 * Builds the 2x3 indirect texture matrix of a scale/rotate/translate set.
 */
void CalcIndTexMtx(f32 mtx[2][3], const TexSRT& texSRT) {
    f32 cosR = math::CosDeg(texSRT.rotate);
    f32 sinR = math::SinDeg(texSRT.rotate);

    mtx[0][0] = cosR * texSRT.scale.x;
    mtx[0][1] = -sinR * texSRT.scale.y;
    mtx[0][2] = texSRT.translate.x;

    mtx[1][0] = sinR * texSRT.scale.x;
    mtx[1][1] = cosR * texSRT.scale.y;
    mtx[1][2] = texSRT.translate.y;
} // no copy in retail (inlined)

/*
 * SetColorComponentValue
 *
 * Stores one component of a colour, clamped to 0..255.
 */
void SetColorComponentValue(ut::Color* pCol, u32 compIdx, s16 value) {
    const u8 u8Val = ut::Min<s16>(ut::Max<s16>(value, 0), 0xFF);

    switch (compIdx) {
    case 0:
        pCol->r = u8Val;
        break;

    case 1:
        pCol->g = u8Val;
        break;

    case 2:
        pCol->b = u8Val;
        break;

    case 3:
        pCol->a = u8Val;
        break;
    }
} // no copy in retail (inlined)

/*
 * SetIndTexMtx
 *
 * Loads an indirect texture matrix, scaled by a power of two into the range the
 * hardware takes (entries below 1 and, where possible, not all below 0.5).
 */
void SetIndTexMtx(GXIndTexMtxID id, const f32 mtx[2][3]) {
    f32 m00, m01, m02;
    f32 m10, m11, m12;

    f32 a00, a01, a02;
    f32 a10, a11, a12;

    s8 scaleExp = 0;

    m00 = mtx[0][0];
    m01 = mtx[0][1];
    m02 = mtx[0][2];
    m10 = mtx[1][0];
    m11 = mtx[1][1];
    m12 = mtx[1][2];

    a00 = math::FAbs(m00);
    a01 = math::FAbs(m01);
    a02 = math::FAbs(m02);
    a10 = math::FAbs(m10);
    a11 = math::FAbs(m11);
    a12 = math::FAbs(m12);

    if (a00 >= 1.0f || a01 >= 1.0f || a02 >= 1.0f || a10 >= 1.0f || a11 >= 1.0f || a12 >= 1.0f) {
        do {
            if (scaleExp >= 46) {
                break;
            }

            ++scaleExp;

            m00 /= 2.0f;
            m01 /= 2.0f;
            m02 /= 2.0f;
            m10 /= 2.0f;
            m11 /= 2.0f;
            m12 /= 2.0f;

            a00 /= 2.0f;
            a01 /= 2.0f;
            a02 /= 2.0f;
            a10 /= 2.0f;
            a11 /= 2.0f;
            a12 /= 2.0f;
        } while (a00 >= 1.0f || a01 >= 1.0f || a02 >= 1.0f || a10 >= 1.0f || a11 >= 1.0f || a12 >= 1.0f);
    } else if (a00 < 0.5f && a01 < 0.5f && a02 < 0.5f && a10 < 0.5f && a11 < 0.5f && a12 < 0.5f) {
        do {
            --scaleExp;

            m00 *= 2.0f;
            m01 *= 2.0f;
            m02 *= 2.0f;
            m10 *= 2.0f;
            m11 *= 2.0f;
            m12 *= 2.0f;

            a00 *= 2.0f;
            a01 *= 2.0f;
            a02 *= 2.0f;
            a10 *= 2.0f;
            a11 *= 2.0f;
            a12 *= 2.0f;
        } while (a00 < 0.5f && a01 < 0.5f && a02 < 0.5f && a10 < 0.5f && a11 < 0.5f && a12 < 0.5f &&
                 scaleExp > -17);
    }

    f32 outMtx[2][3] = {{m00, m01, m02}, {m10, m11, m12}};

    GXSetIndTexMtx(id, outMtx, scaleExp);
} // 0x803C3F90

/*
 * InitTexSRT
 *
 * Sets `num` scale/rotate/translate sets to the identity transform.
 */
void InitTexSRT(TexSRT* texSRTs, u32 num) {
    for (u32 i = 0; i < num; ++i) {
        texSRTs[i].translate = math::VEC2(0.0f, 0.0f);
        texSRTs[i].rotate = 0.0f;
        texSRTs[i].scale = math::VEC2(1.0f, 1.0f);
    }
} // no copy in retail (inlined)

/*
 * The GX memory block holds its arrays back to back, each as long as the count in
 * the bit field says; the offset of an array is the offset of the previous one plus
 * that array's size. The first array (the textures) starts at offset 0.
 */

/*
 * CalcOffsetTexMapAry
 */
u32 CalcOffsetTexMapAry(const detail::BitGXNums& bitGXNums) {
    return 0;
} // no copy in retail (inlined)

/*
 * CalcOffsetTexSRTAry
 */
u32 CalcOffsetTexSRTAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetTexMapAry(bitGXNums) + sizeof(GXTexObj) * bitGXNums.texMap;
} // no copy in retail (inlined)

/*
 * CalcOffsetTexCoordGenAry
 */
u32 CalcOffsetTexCoordGenAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetTexSRTAry(bitGXNums) + sizeof(TexSRT) * bitGXNums.texSRT;
} // no copy in retail (inlined)

/*
 * CalcOffsetChanCtrlAry
 */
u32 CalcOffsetChanCtrlAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetTexCoordGenAry(bitGXNums) + sizeof(TexCoordGen) * bitGXNums.texCoordGen;
} // no copy in retail (inlined)

/*
 * CalcOffsetMatColAry
 */
u32 CalcOffsetMatColAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetChanCtrlAry(bitGXNums) + sizeof(ChanCtrl) * bitGXNums.chanCtrl;
} // no copy in retail (inlined)

/*
 * CalcOffsetTevSwapAry
 */
u32 CalcOffsetTevSwapAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetMatColAry(bitGXNums) + sizeof(ut::Color) * bitGXNums.matCol;
} // no copy in retail (inlined)

/*
 * CalcOffsetGetAlphaCompare
 *
 * The swap table takes a whole word, one byte of table plus three of padding.
 */
u32 CalcOffsetGetAlphaCompare(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetTevSwapAry(bitGXNums) + (sizeof(TevSwapMode) + 3) * bitGXNums.tevSwap;
} // no copy in retail (inlined)

/*
 * CalcOffsetBlendMode
 */
u32 CalcOffsetBlendMode(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetGetAlphaCompare(bitGXNums) + sizeof(AlphaCompare) * bitGXNums.alpComp;
} // no copy in retail (inlined)

/*
 * CalcOffsetIndirectStageAry
 */
u32 CalcOffsetIndirectStageAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetBlendMode(bitGXNums) + sizeof(BlendMode) * bitGXNums.blendMode;
} // no copy in retail (inlined)

/*
 * CalcOffsetIndTexSRTAry
 */
u32 CalcOffsetIndTexSRTAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetIndirectStageAry(bitGXNums) + sizeof(IndirectStage) * bitGXNums.indStage;
} // no copy in retail (inlined)

/*
 * CalcOffsetTevStageAry
 */
u32 CalcOffsetTevStageAry(const detail::BitGXNums& bitGXNums) {
    return CalcOffsetIndTexSRTAry(bitGXNums) + sizeof(TexSRT) * bitGXNums.indSRT;
} // no copy in retail (inlined)

/*
 * CopyGXTexObj
 *
 * Copies a texture object without its user data (the palette pointer).
 */
void CopyGXTexObj(GXTexObj* pDst, const GXTexObj* pSrc) {
    *pDst = *pSrc;
    GXInitTexObjUserData(pDst, NULL);
} // no copy in retail (inlined)

} // namespace

namespace nw4hbm {
namespace lyt {

/*
 * Material::Material
 *
 * An empty material with no name.
 */
Material::Material() {
    Init();
    memset(mName, 0, sizeof(mName));
} // no copy in retail; compiled: it emits the weak ut::Color constructor ahead of the next one

/*
 * Material::Material
 *
 * Builds the material from its data block: colours, then the textures, coordinate
 * generators and TEV settings that follow the counts, copied into GX memory.
 */
Material::Material(const res::Material* pRes, const ResBlockSet& resBlockSet) {
    Init();
    SetName(pRes->name);

    for (int i = 0; i < 3; ++i) {
        mTevCols[i] = pRes->tevCols[i];
    }

    for (int i = 0; i < 4; ++i) {
        mTevKCols[i] = pRes->tevKCols[i];
    }

    u32 resOffs = sizeof(*pRes);

    const res::TexMap* const pResTexMap = detail::ConvertOffsToPtr<res::TexMap>(pRes, resOffs);
    resOffs += sizeof(*pResTexMap) * pRes->resNum.GetTexMapNum();

    const TexSRT* const resTexSRTs = detail::ConvertOffsToPtr<TexSRT>(pRes, resOffs);
    resOffs += sizeof(*resTexSRTs) * pRes->resNum.GetTexSRTNum();

    const TexCoordGen* const resTexCoordGens = detail::ConvertOffsToPtr<TexCoordGen>(pRes, resOffs);
    resOffs += sizeof(*resTexCoordGens) * pRes->resNum.GetTexCoordGenNum();

    u8 texMapNum = ut::Min<u8>(pRes->resNum.GetTexMapNum(), 8);
    u8 texSRTNum = ut::Min<u8>(pRes->resNum.GetTexSRTNum(), 10);
    u8 texCoordGenNum = ut::Min<u8>(pRes->resNum.GetTexCoordGenNum(), 8);
    bool allocChanCtrl = pRes->resNum.GetChanCtrlNum() != 0;
    bool allocMatCol = pRes->resNum.GetMatColNum() != 0;
    bool allocTevSwap = pRes->resNum.HasTevSwapTable();
    bool allocAlpComp = pRes->resNum.HasAlphaCompare();
    bool allocBlendMode = pRes->resNum.HasBlendMode();
    u8 indTexSRTNum = ut::Min<u8>(pRes->resNum.GetIndTexSRTNum(), 3);
    u8 indStageNum = ut::Min<u8>(pRes->resNum.GetIndTexStageNum(), 4);
    u8 tevStageNum = ut::Min<u8>(pRes->resNum.GetTevStageNum(), 16);

    ReserveGXMem(texMapNum, texSRTNum, texCoordGenNum, tevStageNum, allocTevSwap, indStageNum, indTexSRTNum,
                 allocChanCtrl, allocMatCol, allocAlpComp, allocBlendMode);

    if (mpGXMem) {
        SetTextureNum(texMapNum);

        if (texMapNum != 0) {
            const res::Texture* const textures =
                detail::ConvertOffsToPtr<res::Texture>(resBlockSet.pTextureList, sizeof(*resBlockSet.pTextureList));

            GXTexObj* const texMaps = GetTexMapAry();

            for (u8 i = 0; i < mGXMemNum.texMap; ++i) {
                const char* const fileName =
                    detail::ConvertOffsToPtr<char>(textures, textures[pResTexMap[i].texIdx].nameStrOffset);

                void* const pTplRes =
                    resBlockSet.pResAccessor->GetResource(ResourceAccessor::SIGNATURE_TEXTURE_IMAGE, fileName, NULL);

                SetTexture(i, static_cast<TPLPalette*>(pTplRes));

                GXInitTexObjWrapMode(&texMaps[i], static_cast<GXTexWrapMode>(pResTexMap[i].wrapS),
                                     static_cast<GXTexWrapMode>(pResTexMap[i].wrapT));
            }
        }

        TexSRT* texSRTs = GetTexSRTAry();
        for (int i = 0; i < texSRTNum; ++i) {
            texSRTs[i].translate = resTexSRTs[i].translate;
            texSRTs[i].rotate = resTexSRTs[i].rotate;
            texSRTs[i].scale = resTexSRTs[i].scale;
        }

        TexCoordGen* texCoordGens = GetTexCoordGenAry();
        SetTexCoordGenNum(texCoordGenNum);
        for (int i = 0; i < mGXMemNum.texCoordGen; ++i) {
            texCoordGens[i] = resTexCoordGens[i];
        }

        if (allocChanCtrl) {
            const ChanCtrl* const pResChanCtrl = detail::ConvertOffsToPtr<ChanCtrl>(pRes, resOffs);

            *GetChanCtrlAry() = *pResChanCtrl;
            resOffs += sizeof(*pResChanCtrl);
        }

        if (allocMatCol) {
            const ut::Color* const pResMatCol = detail::ConvertOffsToPtr<ut::Color>(pRes, resOffs);

            *GetMatColAry() = *pResMatCol;
            resOffs += sizeof(*pResMatCol);
        }

        if (allocTevSwap) {
            const TevSwapMode* const pResTevSwap = detail::ConvertOffsToPtr<TevSwapMode>(pRes, resOffs);

            TevSwapMode* const tevSwaps = GetTevSwapAry();

            for (int i = 0; i < GX_MAX_TEVSWAP; ++i) {
                tevSwaps[i] = pResTevSwap[i];
            }

            // the table is padded to a whole word in the file as in memory
            resOffs += sizeof(*pResTevSwap) + 3;
        }

        if (indTexSRTNum) {
            TexSRT* const indTexSRTs = GetIndTexSRTAry();
            const TexSRT* const pResIndMtx = detail::ConvertOffsToPtr<TexSRT>(pRes, resOffs);

            for (int i = 0; i < indTexSRTNum; ++i) {
                indTexSRTs[i] = pResIndMtx[i];
            }
        }

        // the file's count is skipped even where more than the cap was stored
        resOffs += sizeof(TexSRT) * pRes->resNum.GetIndTexSRTNum();

        if (indStageNum) {
            SetIndStageNum(indStageNum);

            IndirectStage* const indirectStages = GetIndirectStageAry();
            const IndirectStage* const pResIndStg = detail::ConvertOffsToPtr<IndirectStage>(pRes, resOffs);

            for (int i = 0; i < indStageNum; ++i) {
                indirectStages[i] = pResIndStg[i];
            }
        }

        resOffs += sizeof(IndirectStage) * pRes->resNum.GetIndTexStageNum();

        if (tevStageNum) {
            SetTevStageNum(tevStageNum);

            TevStage* const tevStages = GetTevStageAry();
            const TevStage* const pResTevStg = detail::ConvertOffsToPtr<TevStage>(pRes, resOffs);

            for (int i = 0; i < tevStageNum; ++i) {
                tevStages[i] = pResTevStg[i];
            }
        }

        resOffs += sizeof(TevStage) * pRes->resNum.GetTevStageNum();

        if (allocAlpComp) {
            const AlphaCompare* const pResAlphaCompare = detail::ConvertOffsToPtr<AlphaCompare>(pRes, resOffs);

            *GetAlphaComparePtr() = *pResAlphaCompare;
            resOffs += sizeof(*pResAlphaCompare);
        }

        if (allocBlendMode) {
            const BlendMode* const pResBlendMode = detail::ConvertOffsToPtr<BlendMode>(pRes, resOffs);

            *GetBlendModePtr() = *pResBlendMode;
            resOffs += sizeof(*pResBlendMode);
        }
    }
} // 0x803C41B0

/*
 * Material::Init
 */
void Material::Init() {
    mTevCols[0] = DefaultBlackColor;
    mTevCols[1] = DefaultWhiteColor;
    mTevCols[2] = DefaultWhiteColor;

    InitBitGXNums(&mGXMemCap);
    InitBitGXNums(&mGXMemNum);

    mbUserAllocated = false;
    mpGXMem = NULL;
} // no copy in retail (inlined)

/*
 * Material::~Material
 */
Material::~Material() {
    UnbindAllAnimation();

    if (mpGXMem) {
        Layout::FreeMemory(mpGXMem);
        mpGXMem = NULL;
    }
} // 0x803C5340

/*
 * Material::InitBitGXNums
 */
void Material::InitBitGXNums(detail::BitGXNums* ptr) {
    ptr->texMap = 0;
    ptr->texSRT = 0;
    ptr->texCoordGen = 0;
    ptr->indSRT = 0;
    ptr->indStage = 0;
    ptr->tevSwap = false;
    ptr->tevStage = 0;
    ptr->chanCtrl = false;
    ptr->matCol = false;
    ptr->alpComp = false;
    ptr->blendMode = 0;
} // no copy in retail (inlined)

/*
 * Material::ReserveGXMem
 *
 * Makes sure the GX memory block can hold the given counts, replacing it (and
 * resetting the used counts) when it cannot, and initialises what it now holds.
 */
void Material::ReserveGXMem(u8 texMapNum, u8 texSRTNum, u8 texCoordGenNum, u8 tevStageNum, bool allocTevSwap,
                            u8 indStageNum, u8 indSRTNum, bool allocChanCtrl, bool allocMatCol, bool allocAlpComp,
                            bool allocBlendMode) {
    int tevSwapNum = allocTevSwap ? 1 : 0;
    int chanCtrlNum = allocChanCtrl ? 1 : 0;
    int matColNum = allocMatCol ? 1 : 0;
    int alpCompNum = allocAlpComp ? 1 : 0;
    int blendModeNum = allocBlendMode ? 1 : 0;

    if (mGXMemCap.texMap >= texMapNum && mGXMemCap.texSRT >= texSRTNum && mGXMemCap.texCoordGen >= texCoordGenNum &&
        mGXMemCap.tevStage >= tevStageNum && mGXMemCap.tevSwap >= tevSwapNum && mGXMemCap.indStage >= indStageNum &&
        mGXMemCap.indSRT >= indSRTNum && mGXMemCap.chanCtrl >= chanCtrlNum && mGXMemCap.matCol >= matColNum &&
        mGXMemCap.alpComp >= alpCompNum && mGXMemCap.blendMode >= blendModeNum) {
        return;
    }

    if (mpGXMem) {
        Layout::FreeMemory(mpGXMem);
        mpGXMem = NULL;

        InitBitGXNums(&mGXMemCap);
        InitBitGXNums(&mGXMemNum);
    }

    // The word-sized resources (colours, channel controls, coordinate generators,
    // swap table, alpha compare, blend mode, indirect stages) are one word each.
    mpGXMem = Layout::AllocMemory(sizeof(ut::Color) * (matColNum + chanCtrlNum + texCoordGenNum + tevSwapNum +
                                                       alpCompNum + blendModeNum + indStageNum) +
                                  sizeof(TexSRT) * texSRTNum + sizeof(GXTexObj) * texMapNum +
                                  sizeof(TexSRT) * indSRTNum + sizeof(TevStage) * tevStageNum);

    if (!mpGXMem) {
        return;
    }

    mGXMemCap.texMap = texMapNum;
    mGXMemCap.texSRT = texSRTNum;
    mGXMemCap.texCoordGen = texCoordGenNum;
    mGXMemCap.indSRT = indSRTNum;
    mGXMemCap.indStage = indStageNum;
    mGXMemCap.tevSwap = tevSwapNum;
    mGXMemCap.tevStage = tevStageNum;
    mGXMemCap.chanCtrl = chanCtrlNum;
    mGXMemCap.matCol = matColNum;
    mGXMemCap.alpComp = alpCompNum;
    mGXMemCap.blendMode = blendModeNum;

    mGXMemNum.texSRT = mGXMemCap.texSRT;
    InitTexSRT(GetTexSRTAry(), mGXMemNum.texSRT);

    mGXMemNum.indSRT = mGXMemCap.indSRT;
    InitTexSRT(GetIndTexSRTAry(), mGXMemNum.indSRT);

    mGXMemNum.chanCtrl = mGXMemCap.chanCtrl;
    if (mGXMemNum.chanCtrl) {
        *GetChanCtrlAry() = ChanCtrl();
    }

    mGXMemNum.matCol = mGXMemCap.matCol;
    if (mGXMemNum.matCol) {
        *GetMatColAry() = ut::Color::WHITE;
    }

    mGXMemNum.tevSwap = mGXMemCap.tevSwap;
    if (mGXMemNum.tevSwap) {
        TevSwapMode* tevSwaps = GetTevSwapAry();

        tevSwaps[0].Set(GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
        tevSwaps[1].Set(GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
        tevSwaps[2].Set(GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
        tevSwaps[3].Set(GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
    }

    mGXMemNum.alpComp = mGXMemCap.alpComp;
    if (mGXMemNum.alpComp) {
        *GetAlphaComparePtr() = AlphaCompare();
    }

    mGXMemNum.blendMode = mGXMemCap.blendMode;
    if (mGXMemNum.blendMode) {
        *GetBlendModePtr() = BlendMode();
    }
} // 0x803C53F0

/*
 * Material::GetTexMapAry
 */
const GXTexObj* Material::GetTexMapAry() const {
    return detail::ConvertOffsToPtr<GXTexObj>(mpGXMem, CalcOffsetTexMapAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetTexMapAry
 */
GXTexObj* Material::GetTexMapAry() {
    return detail::ConvertOffsToPtr<GXTexObj>(mpGXMem, CalcOffsetTexMapAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetTexSRTAry
 */
TexSRT* Material::GetTexSRTAry() {
    return detail::ConvertOffsToPtr<TexSRT>(mpGXMem, CalcOffsetTexSRTAry(mGXMemCap));
} // 0x803C5A90

/*
 * Material::GetTexCoordGenAry
 */
TexCoordGen* Material::GetTexCoordGenAry() {
    return detail::ConvertOffsToPtr<TexCoordGen>(mpGXMem, CalcOffsetTexCoordGenAry(mGXMemCap));
} // 0x803C5AB0

/*
 * Material::GetChanCtrlAry
 */
ChanCtrl* Material::GetChanCtrlAry() {
    return detail::ConvertOffsToPtr<ChanCtrl>(mpGXMem, CalcOffsetChanCtrlAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetMatColAry
 */
ut::Color* Material::GetMatColAry() {
    return detail::ConvertOffsToPtr<ut::Color>(mpGXMem, CalcOffsetMatColAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetTevSwapAry
 */
TevSwapMode* Material::GetTevSwapAry() {
    return detail::ConvertOffsToPtr<TevSwapMode>(mpGXMem, CalcOffsetTevSwapAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetAlphaComparePtr
 */
AlphaCompare* Material::GetAlphaComparePtr() {
    return detail::ConvertOffsToPtr<AlphaCompare>(mpGXMem, CalcOffsetGetAlphaCompare(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetBlendModePtr
 */
BlendMode* Material::GetBlendModePtr() {
    return detail::ConvertOffsToPtr<BlendMode>(mpGXMem, CalcOffsetBlendMode(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetIndirectStageAry
 */
IndirectStage* Material::GetIndirectStageAry() {
    return detail::ConvertOffsToPtr<IndirectStage>(mpGXMem, CalcOffsetIndirectStageAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::GetIndTexSRTAry
 */
TexSRT* Material::GetIndTexSRTAry() {
    return detail::ConvertOffsToPtr<TexSRT>(mpGXMem, CalcOffsetIndTexSRTAry(mGXMemCap));
} // 0x803C5AD0

/*
 * Material::GetTevStageAry
 */
TevStage* Material::GetTevStageAry() {
    return detail::ConvertOffsToPtr<TevStage>(mpGXMem, CalcOffsetTevStageAry(mGXMemCap));
} // no copy in retail (inlined)

/*
 * Material::SetName
 */
void Material::SetName(const char* name) {
    strncpy(mName, name, sizeof(mName));
} // no copy in retail (inlined)

/*
 * Material::SetTextureNum
 *
 * Raises the number of textures in use, clearing the new ones.
 */
void Material::SetTextureNum(u8 num) {
    if (num != 0) {
        GXTexObj* texMaps = GetTexMapAry();

        for (u32 i = mGXMemNum.texMap; i < num; ++i) {
            memset(&texMaps[i], 0, sizeof(*texMaps));
        }

        mGXMemNum.texMap = num;
    }
} // 0x803C5B30

/*
 * Material::SetTexCoordGenNum
 *
 * Raises the number of texture coordinate generators in use, giving the new ones
 * the default setting.
 */
void Material::SetTexCoordGenNum(u8 num) {
    if (num != 0) {
        TexCoordGen* const texCoordGens = GetTexCoordGenAry();

        for (u32 i = mGXMemNum.texCoordGen; i < num; ++i) {
            texCoordGens[i] = TexCoordGen();
        }

        mGXMemNum.texCoordGen = num;
    }
} // 0x803C5BC0

/*
 * Material::SetTevStageNum
 *
 * Raises the number of TEV stages in use, giving the new ones the default setting.
 */
void Material::SetTevStageNum(u8 num) {
    if (num != 0) {
        TevStage* tevStages = GetTevStageAry();

        for (u32 i = mGXMemNum.tevStage; i < num; ++i) {
            tevStages[i] = TevStage();
        }

        mGXMemNum.tevStage = num;
    }
} // no copy in retail (inlined)

/*
 * Material::SetIndStageNum
 *
 * Raises the number of indirect stages in use, giving the new ones the default setting.
 */
void Material::SetIndStageNum(u8 num) {
    if (num != 0) {
        IndirectStage* indStages = GetIndirectStageAry();

        for (u32 i = mGXMemNum.indStage; i < num; ++i) {
            indStages[i] = IndirectStage();
        }

        mGXMemNum.indStage = num;
    }
} // no copy in retail (inlined)

/*
 * Material::GetTexture
 */
void Material::GetTexture(GXTexObj* pTexObj, u8 texMapIdx) const {
    CopyGXTexObj(pTexObj, &GetTexMapAry()[texMapIdx]);
} // no copy in retail (inlined)

/*
 * Material::SetTexture
 */
void Material::SetTexture(u8 texMapIdx, TPLPalette* pTplRes) {
    GXTexObj* pDstTexObj = &GetTexMapAry()[texMapIdx];

    detail::InitGXTexObjFromTPL(pDstTexObj, pTplRes, 0);
} // no copy in retail (inlined)

/*
 * Material::SetTextureNoWrap
 *
 * Replaces the texture, keeping the wrap modes of the one it replaces.
 */
void Material::SetTextureNoWrap(u8 texMapIdx, TPLPalette* pTplRes) {
    GXTexObj* pDstTexObj = &GetTexMapAry()[texMapIdx];
    GXTexWrapMode wrapS = GXGetTexObjWrapS(pDstTexObj);
    GXTexWrapMode wrapT = GXGetTexObjWrapT(pDstTexObj);

    detail::InitGXTexObjFromTPL(pDstTexObj, pTplRes, 0);
    GXInitTexObjWrapMode(pDstTexObj, wrapS, wrapT);
} // 0x803C5D20

/*
 * Material::SetTexture
 */
void Material::SetTexture(u8 texMapIdx, const GXTexObj& texObj) {
    GXTexObj* pDstTexObj = &GetTexMapAry()[texMapIdx];

    CopyGXTexObj(pDstTexObj, &texObj);
} // 0x803C5DB0

/*
 * Material::SetColorElement
 *
 * Sets one colour component: types 0-3 are the material colour, 4-15 the three TEV
 * registers and 16-31 the four konst colours (four components each).
 */
void Material::SetColorElement(u32 colorType, s16 value) {
    switch (colorType) {
    case 0 ... 3:
        if (mGXMemNum.matCol >= 1) {
            ut::Color* matCols = GetMatColAry();

            SetColorComponentValue(matCols, colorType & 3, value);
        }

        break;

    case 4 ... 15: {
        u32 regIdx = (colorType - 4) >> 2;

        switch ((colorType - 4) & 3) {
        case 0:
            mTevCols[regIdx].r = value;
            break;

        case 1:
            mTevCols[regIdx].g = value;
            break;

        case 2:
            mTevCols[regIdx].b = value;
            break;

        case 3:
            mTevCols[regIdx].a = value;
            break;
        }
    } break;

    case 16 ... 31: {
        u32 regIdx = (colorType - 16) >> 2;

        SetColorComponentValue(&mTevKCols[regIdx], (colorType - 16) & 3, value);
    } break;
    }
} // 0x803C5E10

/*
 * Material::SetupGX
 *
 * Programs the GX pipeline with the material's channels, textures, TEV stages and
 * blend settings. Without TEV stages of its own the material builds the stages its
 * textures call for. Returns whether the vertex colours take part in the result.
 */
bool Material::SetupGX(bool bModVtxCol, u8 alpha) {
    // Konst colour and alpha selections that pick one channel of konst colour 3 or 2
    // for each of up to eight textures.
    static GXTevKColorSel kColSels[8] = {GX_TEV_KCSEL_K3_A, GX_TEV_KCSEL_K3_B, GX_TEV_KCSEL_K3_G, GX_TEV_KCSEL_K3_R,
                                         GX_TEV_KCSEL_K2_A, GX_TEV_KCSEL_K2_B, GX_TEV_KCSEL_K2_G, GX_TEV_KCSEL_K2_R};

    static GXTevKAlphaSel kAlpSels[8] = {GX_TEV_KASEL_K3_A, GX_TEV_KASEL_K3_B, GX_TEV_KASEL_K3_G, GX_TEV_KASEL_K3_R,
                                         GX_TEV_KASEL_K2_A, GX_TEV_KASEL_K2_B, GX_TEV_KASEL_K2_G, GX_TEV_KASEL_K2_R};

    bool bUseVtxCol = true;
    bool bUseMatCol = false;

    GXSetNumChans(1);

    if (IsChanCtrlCap()) {
        ChanCtrl* chanCtrls = GetChanCtrlAry();

        GXSetChanCtrl(GX_COLOR0, false, GX_SRC_REG, chanCtrls->GetColorSrc(), GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
        GXSetChanCtrl(GX_ALPHA0, false, GX_SRC_REG, chanCtrls->GetAlphaSrc(), GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);

        bUseVtxCol = chanCtrls->GetColorSrc() == GX_SRC_VTX || chanCtrls->GetAlphaSrc() == GX_SRC_VTX;
        bUseMatCol = chanCtrls->GetColorSrc() == GX_SRC_REG || chanCtrls->GetAlphaSrc() == GX_SRC_REG;
    } else {
        GXSetChanCtrl(GX_COLOR0A0, false, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    }

    bool bUseRasStage = false;

    if (bUseVtxCol) {
        bUseRasStage = bUseRasStage || bModVtxCol;
    }

    if (bUseMatCol) {
        ut::Color matCol(ut::Color::WHITE);

        if (IsMatColorCap()) {
            matCol = *GetMatColAry();
        }

        matCol = detail::MultipleAlpha(matCol, alpha);
        GXSetChanMatColor(GX_COLOR0A0, matCol);

        bUseRasStage = bUseRasStage || matCol != ut::Color::WHITE;
    }

    bool bSetTexMtx = false;
    bool bUseTexMtx[10];

    for (int i = 0; i < 10; ++i) {
        bUseTexMtx[i] = false;
    }

    GXSetNumTexGens(mGXMemNum.texCoordGen);

    if (mGXMemNum.texCoordGen) {
        TexCoordGen* texCoordGens = GetTexCoordGenAry();

        for (int i = 0; i < mGXMemNum.texCoordGen; ++i) {
            u32 texMtx = texCoordGens[i].GetTexMtx();

            if (texCoordGens[i].GetTexGenType() == GX_TG_MTX2x4 && texMtx != GX_IDENTITY) {
                bUseTexMtx[GetTexMtxIdx(texMtx)] = true;
                bSetTexMtx = true;
            }

            GXSetTexCoordGen(static_cast<GXTexCoordID>(i), texCoordGens[i].GetTexGenType(),
                             texCoordGens[i].GetTexGenSrc(), texMtx);
        }
    }

    if (bSetTexMtx) {
        TexSRT* const texSRTs = GetTexSRTAry();
        for (u8 i = 0; i < mGXMemNum.texSRT; ++i) {
            if (bUseTexMtx[i]) {
                math::MTX34 texMtx;
                CalcTextureMtx(&texMtx, texSRTs[i]);

                GXLoadTexMtxImm(texMtx.m, GetTexMtx(i), GX_MTX2x4);
            }
        }
    }

    if (mGXMemNum.texMap) {
        u32 tlutName = 0;
        GXTexObj* texMaps = GetTexMapAry();

        for (int i = 0; i < mGXMemNum.texMap; ++i) {
            GXTexObj texMap = texMaps[i];
            int texFmt = GXGetTexObjFmt(&texMap);

            // Indexed formats (CI4, CI8) load their palette first.
            if (texFmt == GX_TF_C4 || texFmt == GX_TF_C8) {
                TPLClutHeader* pClutHeader = static_cast<TPLClutHeader*>(GXGetTexObjUserData(&texMap));

                if (pClutHeader) {
                    GXTlutObj tlutObj;

                    GXInitTlutObj(&tlutObj, pClutHeader->data, pClutHeader->format, pClutHeader->numEntries);
                    GXLoadTlut(&tlutObj, tlutName);
                    GXInitTexObjTlut(&texMap, tlutName);

                    ++tlutName;
                }
            }

            GXLoadTexObj(&texMap, static_cast<GXTexMapID>(i));
        }
    }

    GXSetTevColorS10(GX_TEVREG0, mTevCols[0]);
    GXSetTevColorS10(GX_TEVREG1, mTevCols[1]);
    GXSetTevColorS10(GX_TEVREG2, mTevCols[2]);

    GXSetTevKColor(GX_KCOLOR0, mTevKCols[0]);
    GXSetTevKColor(GX_KCOLOR1, mTevKCols[1]);
    GXSetTevKColor(GX_KCOLOR2, mTevKCols[2]);
    GXSetTevKColor(GX_KCOLOR3, mTevKCols[3]);

    if (IsTevSwapCap()) {
        TevSwapMode* tevSwaps = GetTevSwapAry();

        for (int i = 0; i < 4; ++i) {
            GXSetTevSwapModeTable(static_cast<GXTevSwapSel>(i), tevSwaps[i].GetR(), tevSwaps[i].GetG(),
                                  tevSwaps[i].GetB(), tevSwaps[i].GetA());
        }
    } else {
        GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
        GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
        GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
        GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
    }

    bool bSetIndTexMtx = false;
    bool bUseIndTexMtx[3];

    for (int i = 0; i < 3; ++i) {
        bUseIndTexMtx[i] = false;
    }

    if (mGXMemNum.tevStage) {
        GXSetNumTevStages(mGXMemNum.tevStage);

        TevStage* tevStages = GetTevStageAry();
        for (int i = 0; i < mGXMemNum.tevStage; ++i) {
            GXTevStageID tevStage = static_cast<GXTevStageID>(i);

            GXSetTevOrder(tevStage, tevStages[i].GetTexCoordGen(), tevStages[i].GetTexMap(),
                          tevStages[i].GetColorChan());

            GXSetTevSwapMode(tevStage, tevStages[i].GetRasSwapSel(), tevStages[i].GetTexSwapSel());

            GXSetTevColorIn(tevStage, tevStages[i].GetColorInA(), tevStages[i].GetColorInB(),
                            tevStages[i].GetColorInC(), tevStages[i].GetColorInD());

            GXSetTevColorOp(tevStage, tevStages[i].GetColorOp(), tevStages[i].GetColorBias(),
                            tevStages[i].GetColorScale(), tevStages[i].IsColorClamp(), tevStages[i].GetColorOutReg());

            GXSetTevKColorSel(tevStage, tevStages[i].GetKColorSel());

            GXSetTevAlphaIn(tevStage, tevStages[i].GetAlphaInA(), tevStages[i].GetAlphaInB(),
                            tevStages[i].GetAlphaInC(), tevStages[i].GetAlphaInD());

            GXSetTevAlphaOp(tevStage, tevStages[i].GetAlphaOp(), tevStages[i].GetAlphaBias(),
                            tevStages[i].GetAlphaScale(), tevStages[i].IsAlphaClamp(), tevStages[i].GetAlphaOutReg());

            GXSetTevKAlphaSel(tevStage, tevStages[i].GetKAlphaSel());

            GXIndTexMtxID indMtxSel = tevStages[i].GetIndMtxSel();
            GXSetTevIndirect(tevStage, tevStages[i].GetIndStage(), tevStages[i].GetIndFormat(),
                             tevStages[i].GetIndBiasSel(), indMtxSel, tevStages[i].GetIndWrapS(),
                             tevStages[i].GetIndWrapT(), tevStages[i].IsIndAddPrev(), tevStages[i].IsIndUtcLod(),
                             tevStages[i].GetIndAlphaSel());

            if (GX_ITM_0 <= indMtxSel && indMtxSel <= GX_ITM_2) {
                bUseIndTexMtx[indMtxSel - 1] = true;
                bSetIndTexMtx = true;
            }
        }

        bUseRasStage = true;
    } else {
        u8 tevStageID = 0;

        if (mGXMemNum.texMap == 0) {
            GXTevStageID tevStage = static_cast<GXTevStageID>(tevStageID);

            GXSetTevOrder(tevStage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
            GXSetTevColorIn(tevStage, GX_CC_ZERO, GX_CC_C1, GX_CC_RASC, GX_CC_ZERO);
            GXSetTevAlphaIn(tevStage, GX_CA_ZERO, GX_CA_A1, GX_CA_RASA, GX_CA_ZERO);

            ++tevStageID;
            bUseRasStage = true;
        } else {
            if (mGXMemNum.texMap == 1) {
                GXTevStageID tevStage = static_cast<GXTevStageID>(tevStageID);

                GXSetTevOrder(tevStage, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
                GXSetTevColorIn(tevStage, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
                GXSetTevAlphaIn(tevStage, GX_CA_A0, GX_CA_A1, GX_CA_TEXA, GX_CA_ZERO);

                ++tevStageID;
            } else {
                if (mGXMemNum.texMap == 2) {
                    GXTevStageID tevStage;

                    tevStage = static_cast<GXTevStageID>(tevStageID);
                    GXSetTevOrder(tevStage, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
                    GXSetTevColorIn(tevStage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
                    GXSetTevAlphaIn(tevStage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

                    ++tevStageID;

                    tevStage = static_cast<GXTevStageID>(tevStageID);
                    GXSetTevOrder(tevStage, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
                    GXSetTevColorIn(tevStage, GX_CC_TEXC, GX_CC_CPREV, GX_CC_KONST, GX_CC_ZERO);
                    GXSetTevAlphaIn(tevStage, GX_CA_TEXA, GX_CA_APREV, GX_CA_KONST, GX_CA_ZERO);

                    GXSetTevKColorSel(tevStage, kColSels[0]);
                    GXSetTevKAlphaSel(tevStage, kAlpSels[0]);

                    ++tevStageID;
                } else {
                    for (int i = 0; i < mGXMemNum.texMap; ++i) {
                        GXTevStageID tevStage = static_cast<GXTevStageID>(tevStageID);

                        GXSetTevOrder(tevStage, static_cast<GXTexCoordID>(i), static_cast<GXTexMapID>(i),
                                      GX_COLOR_NULL);

                        GXTevColorArg colDIn = i == 0 ? GX_CC_ZERO : GX_CC_CPREV;
                        GXTevAlphaArg alpDIn = i == 0 ? GX_CA_ZERO : GX_CA_APREV;

                        GXSetTevColorIn(tevStage, GX_CC_ZERO, GX_CC_TEXC, GX_CC_KONST, colDIn);
                        GXSetTevAlphaIn(tevStage, GX_CA_ZERO, GX_CA_TEXA, GX_CA_KONST, alpDIn);

                        GXSetTevKColorSel(tevStage, kColSels[i]);
                        GXSetTevKAlphaSel(tevStage, kAlpSels[i]);

                        ++tevStageID;
                    }
                }

                if (mTevCols[0] != DefaultBlackColor || mTevCols[1] != DefaultWhiteColor) {
                    GXTevStageID tevStage = static_cast<GXTevStageID>(tevStageID);

                    GXSetTevOrder(tevStage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
                    GXSetTevColorIn(tevStage, GX_CC_C0, GX_CC_C1, GX_CC_CPREV, GX_CC_ZERO);
                    GXSetTevAlphaIn(tevStage, GX_CA_A0, GX_CA_A1, GX_CA_APREV, GX_CA_ZERO);

                    ++tevStageID;
                }
            }

            if (bUseRasStage) {
                GXTevStageID tevStage = static_cast<GXTevStageID>(tevStageID);

                GXSetTevOrder(tevStage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
                GXSetTevColorIn(tevStage, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASC, GX_CC_ZERO);
                GXSetTevAlphaIn(tevStage, GX_CA_ZERO, GX_CA_APREV, GX_CA_RASA, GX_CA_ZERO);

                ++tevStageID;
            }
        }

        const u8 tevStageNum = tevStageID;
        for (u8 id = 0; id < tevStageNum; ++id) {
            GXTevStageID tevStage = static_cast<GXTevStageID>(id);

            GXSetTevColorOp(tevStage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
            GXSetTevAlphaOp(tevStage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);

            GXSetTevDirect(tevStage);
            GXSetTevSwapMode(tevStage, GX_TEV_SWAP0, GX_TEV_SWAP0);
        }

        GXSetNumTevStages(tevStageNum);
    }

    if (bSetIndTexMtx) {
        TexSRT* indTexSRTs = GetIndTexSRTAry();

        for (int i = 0; i < mGXMemNum.indSRT; ++i) {
            if (bUseIndTexMtx[i]) {
                f32 mtx[2][3];

                CalcIndTexMtx(mtx, indTexSRTs[i]);
                SetIndTexMtx(static_cast<GXIndTexMtxID>(i + 1), mtx);
            }
        }
    }

    GXSetNumIndStages(mGXMemNum.indStage);

    if (mGXMemNum.indStage) {
        IndirectStage* indirectStages = GetIndirectStageAry();

        for (int i = 0; i < mGXMemNum.indStage; ++i) {
            GXIndTexStageID indStage = static_cast<GXIndTexStageID>(i);

            GXSetIndTexOrder(indStage, indirectStages[i].GetTexCoordGen(), indirectStages[i].GetTexMap());

            GXSetIndTexCoordScale(indStage, indirectStages[i].GetScaleS(), indirectStages[i].GetScaleT());
        }
    }

    if (IsAlphaCompareCap()) {
        AlphaCompare* pAlpComp = GetAlphaComparePtr();

        GXSetAlphaCompare(pAlpComp->GetComp0(), pAlpComp->GetRef0(), pAlpComp->GetOp(), pAlpComp->GetComp1(),
                          pAlpComp->GetRef1());
    } else {
        GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    }

    if (IsBlendModeCap()) {
        BlendMode* pBlendMode = GetBlendModePtr();

        GXSetBlendMode(pBlendMode->GetType(), pBlendMode->GetSrcFactor(), pBlendMode->GetDstFactor(),
                       pBlendMode->GetOp());
    } else {
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    }

    return bUseRasStage && bUseVtxCol;
} // 0x803C5FD0

/*
 * Material::BindAnimation
 */
void Material::BindAnimation(AnimTransform* pAnimTrans) {
    pAnimTrans->Bind(this);
} // 0x803C6DF0

/*
 * Material::UnbindAnimation
 *
 * Unlinks `pAnimTrans` from the material; NULL unlinks every animation.
 */
void Material::UnbindAnimation(AnimTransform* pAnimTrans) {
    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mAnimList) {
        __typeof__(it) currIt = it++;

        if (pAnimTrans == NULL || currIt->GetAnimTransform() == pAnimTrans) {
            mAnimList.Erase(currIt);
            currIt->Reset();
        }
    }
} // 0x803C6E10

/*
 * Material::UnbindAllAnimation
 */
void Material::UnbindAllAnimation() {
    UnbindAnimation(NULL);
} // 0x803C6E90

/*
 * Material::Animate
 */
void Material::Animate() {
    NW4HBM_RANGE_FOR(it, mAnimList) {
        if (it->IsEnable()) {
            AnimTransform* animTrans = it->GetAnimTransform();

            animTrans->Animate(it->GetIndex(), this);
        }
    }
} // 0x803C6EB0

/*
 * Material::AddAnimationLink
 */
void Material::AddAnimationLink(AnimationLink* pAnimationLink) {
    mAnimList.PushBack(pAnimationLink);
} // 0x803C6F30

/*
 * Material::FindAnimationLink
 */
AnimationLink* Material::FindAnimationLink(AnimTransform* pAnimTrans) {
    return detail::FindAnimationLink(&mAnimList, pAnimTrans);
} // 0x803C6F70

/*
 * Material::SetAnimationEnable
 */
void Material::SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable) {
    if (AnimationLink* pAnimLink = FindAnimationLink(pAnimTrans)) {
        pAnimLink->SetEnable(bEnable);
    }
} // 0x803C6F80

/*
 * detail::GetTextureSize
 */
Size detail::GetTextureSize(Material* pMaterial, u8 texMapIdx) {
    if (texMapIdx >= pMaterial->GetTextureNum()) {
        return Size(0.0f, 0.0f);
    }

    GXTexObj texObj;
    pMaterial->GetTexture(&texObj, texMapIdx);

    return Size(GXGetTexObjWidth(&texObj), GXGetTexObjHeight(&texObj));
} // 0x803C6FD0

} // namespace lyt
} // namespace nw4hbm
