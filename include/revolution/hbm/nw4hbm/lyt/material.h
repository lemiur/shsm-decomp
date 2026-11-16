#ifndef NW4HBM_LYT_MATERIAL_H
#define NW4HBM_LYT_MATERIAL_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/material.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_material.h

#include <revolution/types.h>
#include <revolution/gx.h>
#include <revolution/tpl.h>
#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"

namespace nw4hbm {
namespace lyt {

struct ResBlockSet;

/*
 * MaterialResourceNum
 *
 * Packed counts of the GX resources a material block carries.
 */
class MaterialResourceNum {
public:
    u8 GetTexMapNum() const { return detail::GetBits(bits, 0, 4); }
    u8 GetTexSRTNum() const { return detail::GetBits(bits, 4, 4); }
    u8 GetTexCoordGenNum() const { return detail::GetBits(bits, 8, 4); }
    bool HasTevSwapTable() const { return detail::TestBit(bits, 12); }
    u8 GetIndTexSRTNum() const { return detail::GetBits(bits, 13, 2); }
    u8 GetIndTexStageNum() const { return detail::GetBits(bits, 15, 3); }
    u8 GetTevStageNum() const { return detail::GetBits(bits, 18, 5); }
    bool HasAlphaCompare() const { return detail::TestBit(bits, 23); }
    bool HasBlendMode() const { return detail::TestBit(bits, 24); }
    u8 GetChanCtrlNum() const { return detail::GetBits(bits, 25, 1); }
    u8 GetMatColNum() const { return detail::GetBits(bits, 27, 1); }

private:
    u32 bits; // 0x00
}; // size 0x04

namespace res {

/*
 * res::Material
 *
 * Layout-file data block of a material; its resources follow the counts.
 */
struct Material {
    char name[20];              // 0x00
    GXColorS10 tevCols[3];      // 0x14
    GXColor tevKCols[4];        // 0x2C
    MaterialResourceNum resNum; // 0x3C
}; // size 0x40

/*
 * res::Texture
 *
 * One entry of the texture list: where the texture's file name is stored.
 */
struct Texture {
    u32 nameStrOffset; // 0x00
    u8 type;           // 0x04
    u8 padding[3];     // 0x05 // pad: SDK res::Texture.padding
}; // size 0x08

/*
 * res::TexMap
 *
 * How a material uses one texture: which one and how it wraps.
 */
struct TexMap {
    u16 texIdx; // 0x00
    u8 wrapS;   // 0x02 GXTexWrapMode
    u8 wrapT;   // 0x03 GXTexWrapMode
}; // size 0x04

} // namespace res

namespace detail {

/*
 * BitGXNums
 *
 * How many of each GX resource a material has (or has room for).
 */
struct BitGXNums {
    u32 texMap : 4;
    u32 texSRT : 4;
    u32 texCoordGen : 4;
    u32 indSRT : 2;
    u32 indStage : 3;
    u32 tevSwap : 1;
    u32 tevStage : 5;
    u32 chanCtrl : 1;
    u32 matCol : 1;
    u32 alpComp : 1;
    u32 blendMode : 1;
}; // size 0x04

} // namespace detail

/*
 * TexSRT
 *
 * Scale, rotation and translation of a texture coordinate set.
 */
struct TexSRT {
    math::VEC2 translate; // 0x00
    f32 rotate;           // 0x08
    math::VEC2 scale;     // 0x0C
}; // size 0x14

/*
 * TexCoordGen
 *
 * How a material generates one set of texture coordinates.
 */
class TexCoordGen {
public:
    TexCoordGen() : reserve() { Set(GX_TG_MTX2x4, GX_TG_TEX0, 60); }

    GXTexGenType GetTexGenType() const { return static_cast<GXTexGenType>(texGenType); }
    GXTexGenSrc GetTexGenSrc() const { return static_cast<GXTexGenSrc>(texGenSrc); }
    u32 GetTexMtx() const { return texMtx; }

    void Set(GXTexGenType aTexGenType, GXTexGenSrc aTexGenSrc, u32 aTexMtx) {
        texGenType = aTexGenType;
        texGenSrc = aTexGenSrc;
        texMtx = aTexMtx;
    }

private:
    u8 texGenType; // 0x00 GXTexGenType
    u8 texGenSrc;  // 0x01 GXTexGenSrc
    u8 texMtx;     // 0x02
    u8 reserve;    // 0x03 // pad: SDK TexCoordGen.reserve
}; // size 0x04

/*
 * IndirectStage
 *
 * The texture coordinates and map one indirect texture stage reads, with its scales.
 */
class IndirectStage {
public:
    IndirectStage() { Set(GX_TEXCOORD0, GX_TEXMAP0, GX_ITS_1, GX_ITS_1); }

    GXTexCoordID GetTexCoordGen() const { return static_cast<GXTexCoordID>(texCoordGen); }
    GXTexMapID GetTexMap() const { return static_cast<GXTexMapID>(texMap); }
    GXIndTexScale GetScaleS() const { return static_cast<GXIndTexScale>(scaleS); }
    GXIndTexScale GetScaleT() const { return static_cast<GXIndTexScale>(scaleT); }

    void Set(GXTexCoordID aTexCoordGen, GXTexMapID aTexMap, GXIndTexScale aScaleS, GXIndTexScale aScaleT) {
        texCoordGen = aTexCoordGen;
        texMap = aTexMap;
        scaleS = aScaleS;
        scaleT = aScaleT;
    }

private:
    u8 texCoordGen; // 0x00 GXTexCoordID
    u8 texMap;      // 0x01 GXTexMapID
    u8 scaleS;      // 0x02 GXIndTexScale
    u8 scaleT;      // 0x03 GXIndTexScale
}; // size 0x04

/*
 * TevSwapMode
 *
 * One entry of the TEV swap table: the colour channel each output channel reads.
 */
class TevSwapMode {
public:
    GXTevColorChan GetR() const { return static_cast<GXTevColorChan>(swap & 0x03); }
    GXTevColorChan GetG() const { return static_cast<GXTevColorChan>((swap >> 2) & 0x03); }
    GXTevColorChan GetB() const { return static_cast<GXTevColorChan>((swap >> 4) & 0x03); }
    GXTevColorChan GetA() const { return static_cast<GXTevColorChan>((swap >> 6) & 0x03); }

    void Set(GXTevColorChan r, GXTevColorChan g, GXTevColorChan b, GXTevColorChan a) {
        swap = r | g << 2 | b << 4 | a << 6;
    }

private:
    u8 swap; // 0x00 two bits per channel, R in the low bits
}; // size 0x01

/*
 * TevStageInOp
 *
 * The inputs, operation and output of one half (colour or alpha) of a TEV stage.
 */
class TevStageInOp {
public:
    u8 GetA() const { return ab & 0x0F; }
    u8 GetB() const { return (ab >> 4) & 0x0F; }
    u8 GetC() const { return cd & 0x0F; }
    u8 GetD() const { return (cd >> 4) & 0x0F; }

    u8 GetScale() const { return (op >> 6) & 0x03; }
    u8 GetBias() const { return (op >> 4) & 0x03; }
    u8 GetOp() const { return op & 0x0F; }

    u8 GetKSel() const { return (cl >> 3) & 0x1F; }
    u8 GetOutReg() const { return (cl >> 1) & 0x03; }
    bool IsClamp() const { return static_cast<bool>(cl & 0x01); }

    void SetIn(u8 a, u8 b, u8 c, u8 d) {
        ab = a | b << 4;
        cd = c | d << 4;
    }

    void SetOp(u8 aOp, u8 bias, u8 scale, bool clamp, u8 outReg, u8 kSel) {
        op = aOp | bias << 4 | scale << 6;
        cl = (clamp ? 1 : 0) << 0 | outReg << 1 | kSel << 3;
    }

private:
    u8 ab; // 0x00 inputs A (high nibble) and B (low nibble)
    u8 cd; // 0x01 inputs C and D
    u8 op; // 0x02 scale, bias and operation
    u8 cl; // 0x03 konst selection, output register and clamp
}; // size 0x04

/*
 * TevStage
 *
 * One TEV stage: texture and colour order, colour and alpha combiners, indirect setup.
 */
class TevStage {
public:
    TevStage() {
        SetOrder(GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0, GX_TEV_SWAP0, GX_TEV_SWAP0);
        SetColorIn(GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
        SetAlphaIn(GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
        SetColorOp(GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV, GX_TEV_KCSEL_K0);
        SetAlphaOp(GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, true, GX_TEVPREV, GX_TEV_KASEL_K0_R);
        SetIndirect(GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF, false, false,
                    GX_ITBA_OFF);
    }

    GXTexCoordID GetTexCoordGen() const { return static_cast<GXTexCoordID>(texCoordGen); }
    GXChannelID GetColorChan() const { return static_cast<GXChannelID>(colChan); }

    // The low bit of swapSel is bit 8 of the map ID (GX_TEXMAP_DISABLE).
    GXTexMapID GetTexMap() const { return static_cast<GXTexMapID>((swapSel & 1) << 8 | texMap); }

    GXTevSwapSel GetTexSwapSel() const { return static_cast<GXTevSwapSel>((swapSel >> 3) & 0x03); }
    GXTevSwapSel GetRasSwapSel() const { return static_cast<GXTevSwapSel>((swapSel >> 1) & 0x03); }

    GXTevColorArg GetColorInA() const { return static_cast<GXTevColorArg>(colIn.GetA()); }
    GXTevColorArg GetColorInB() const { return static_cast<GXTevColorArg>(colIn.GetB()); }
    GXTevColorArg GetColorInC() const { return static_cast<GXTevColorArg>(colIn.GetC()); }
    GXTevColorArg GetColorInD() const { return static_cast<GXTevColorArg>(colIn.GetD()); }
    GXTevOp GetColorOp() const { return static_cast<GXTevOp>(colIn.GetOp()); }
    GXTevBias GetColorBias() const { return static_cast<GXTevBias>(colIn.GetBias()); }
    GXTevScale GetColorScale() const { return static_cast<GXTevScale>(colIn.GetScale()); }
    bool IsColorClamp() const { return colIn.IsClamp(); }
    GXTevRegID GetColorOutReg() const { return static_cast<GXTevRegID>(colIn.GetOutReg()); }
    GXTevKColorSel GetKColorSel() const { return static_cast<GXTevKColorSel>(colIn.GetKSel()); }

    GXTevAlphaArg GetAlphaInA() const { return static_cast<GXTevAlphaArg>(alpIn.GetA()); }
    GXTevAlphaArg GetAlphaInB() const { return static_cast<GXTevAlphaArg>(alpIn.GetB()); }
    GXTevAlphaArg GetAlphaInC() const { return static_cast<GXTevAlphaArg>(alpIn.GetC()); }
    GXTevAlphaArg GetAlphaInD() const { return static_cast<GXTevAlphaArg>(alpIn.GetD()); }
    GXTevOp GetAlphaOp() const { return static_cast<GXTevOp>(alpIn.GetOp()); }
    GXTevBias GetAlphaBias() const { return static_cast<GXTevBias>(alpIn.GetBias()); }
    GXTevScale GetAlphaScale() const { return static_cast<GXTevScale>(alpIn.GetScale()); }
    bool IsAlphaClamp() const { return alpIn.IsClamp(); }
    GXTevRegID GetAlphaOutReg() const { return static_cast<GXTevRegID>(alpIn.GetOutReg()); }
    GXTevKAlphaSel GetKAlphaSel() const { return static_cast<GXTevKAlphaSel>(alpIn.GetKSel()); }

    GXIndTexStageID GetIndStage() const { return static_cast<GXIndTexStageID>(indStage); }

    GXIndTexMtxID GetIndMtxSel() const { return static_cast<GXIndTexMtxID>((indBiMt >> 3) & 0x0F); }
    GXIndTexBiasSel GetIndBiasSel() const { return static_cast<GXIndTexBiasSel>(indBiMt & 0x07); }

    GXIndTexWrap GetIndWrapS() const { return static_cast<GXIndTexWrap>(indWrap & 0x07); }
    GXIndTexWrap GetIndWrapT() const { return static_cast<GXIndTexWrap>((indWrap >> 3) & 0x07); }

    GXIndTexAlphaSel GetIndAlphaSel() const { return static_cast<GXIndTexAlphaSel>((indFoAdUtAl >> 4) & 0x03); }
    bool IsIndUtcLod() const { return static_cast<bool>((indFoAdUtAl >> 3) & 0x01); }
    bool IsIndAddPrev() const { return static_cast<bool>((indFoAdUtAl >> 2) & 0x01); }
    GXIndTexFormat GetIndFormat() const { return static_cast<GXIndTexFormat>(indFoAdUtAl & 0x03); }

    void SetOrder(GXTexCoordID aTexCoordGen, GXTexMapID aTexMap, GXChannelID aColChan, GXTevSwapSel rasSel,
                  GXTevSwapSel texSel) {
        texCoordGen = aTexCoordGen;
        colChan = aColChan;
        texMap = aTexMap;
        swapSel = aTexMap >> 8 | rasSel << 1 | texSel << 3;
    }

    void SetColorIn(GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) { colIn.SetIn(a, b, c, d); }

    void SetColorOp(GXTevOp op, GXTevBias bias, GXTevScale scale, bool clamp, GXTevRegID outReg,
                    GXTevKColorSel kSel) {
        colIn.SetOp(op, bias, scale, clamp, outReg, kSel);
    }

    void SetAlphaIn(GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) { alpIn.SetIn(a, b, c, d); }

    void SetAlphaOp(GXTevOp op, GXTevBias bias, GXTevScale scale, bool clamp, GXTevRegID outReg,
                    GXTevKAlphaSel kSel) {
        alpIn.SetOp(op, bias, scale, clamp, outReg, kSel);
    }

    void SetIndirect(GXIndTexStageID stage, GXIndTexFormat format, GXIndTexBiasSel biasSel, GXIndTexMtxID mtxSel,
                     GXIndTexWrap wrapS, GXIndTexWrap wrapT, bool addPrev, bool utcLod,
                     GXIndTexAlphaSel alphaSel) {
        indStage = stage;
        indBiMt = biasSel | mtxSel << 3;
        indWrap = wrapS | wrapT << 3;
        indFoAdUtAl = format | (addPrev ? 1 : 0) << 2 | (utcLod ? 1 : 0) << 3 | alphaSel << 4;
    }

private:
    u8 texCoordGen;     // 0x00 GXTexCoordID
    u8 colChan;         // 0x01 GXChannelID
    u8 texMap;          // 0x02 GXTexMapID, low byte
    u8 swapSel;         // 0x03 bit 0: map ID bit 8; bits 1-2: raster swap; bits 3-4: texture swap
    TevStageInOp colIn; // 0x04
    TevStageInOp alpIn; // 0x08
    u8 indStage;        // 0x0C GXIndTexStageID
    u8 indBiMt;         // 0x0D bits 0-2: bias; bits 3-6: matrix
    u8 indWrap;         // 0x0E bits 0-2: wrap S; bits 3-5: wrap T
    u8 indFoAdUtAl;     // 0x0F bits 0-1: format; bit 2: add previous; bit 3: UTC LOD; bits 4-5: alpha select
}; // size 0x10

/*
 * ChanCtrl
 *
 * Where the colour and alpha channels of a material take their material colour from.
 */
class ChanCtrl {
public:
    ChanCtrl() : reserve1(), reserve2() { Set(GX_SRC_VTX, GX_SRC_VTX); }

    GXColorSrc GetColorSrc() const { return static_cast<GXColorSrc>(matSrcCol); }
    GXColorSrc GetAlphaSrc() const { return static_cast<GXColorSrc>(matSrcAlp); }

    void Set(GXColorSrc colSrc, GXColorSrc alpSrc) {
        matSrcCol = colSrc;
        matSrcAlp = alpSrc;
    }

private:
    u8 matSrcCol; // 0x00 GXColorSrc
    u8 matSrcAlp; // 0x01 GXColorSrc
    u8 reserve1;  // 0x02 // pad: SDK ChanCtrl.reserve1
    u8 reserve2;  // 0x03 // pad: SDK ChanCtrl.reserve2
}; // size 0x04

/*
 * AlphaCompare
 *
 * The two alpha tests of a material and how their results combine.
 */
class AlphaCompare {
public:
    AlphaCompare() { Set(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0); }

    GXCompare GetComp0() const { return static_cast<GXCompare>(comp & 0x0F); }
    GXCompare GetComp1() const { return static_cast<GXCompare>(comp >> 4 & 0x0F); }
    GXAlphaOp GetOp() const { return static_cast<GXAlphaOp>(op); }
    u8 GetRef0() const { return ref0; }
    u8 GetRef1() const { return ref1; }

    void Set(GXCompare aComp0, u8 aRef0, GXAlphaOp aOp, GXCompare aComp1, u8 aRef1) {
        comp = aComp0 | aComp1 << 4;
        op = aOp;
        ref0 = aRef0;
        ref1 = aRef1;
    }

private:
    u8 comp; // 0x00 test 0 (low nibble) and test 1 (high nibble)
    u8 op;   // 0x01 GXAlphaOp
    u8 ref0; // 0x02
    u8 ref1; // 0x03
}; // size 0x04

/*
 * BlendMode
 *
 * How a material's pixels combine with the frame buffer.
 */
class BlendMode {
public:
    BlendMode() { Set(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET); }

    GXBlendMode GetType() const { return static_cast<GXBlendMode>(type); }
    GXBlendFactor GetSrcFactor() const { return static_cast<GXBlendFactor>(srcFactor); }
    GXBlendFactor GetDstFactor() const { return static_cast<GXBlendFactor>(dstFactor); }
    GXLogicOp GetOp() const { return static_cast<GXLogicOp>(op); }

    void Set(GXBlendMode aType, GXBlendFactor aSrcFactor, GXBlendFactor aDstFactor, GXLogicOp aOp) {
        type = aType;
        srcFactor = aSrcFactor;
        dstFactor = aDstFactor;
        op = aOp;
    }

private:
    u8 type;      // 0x00 GXBlendMode
    u8 srcFactor; // 0x01 GXBlendFactor
    u8 dstFactor; // 0x02 GXBlendFactor
    u8 op;        // 0x03 GXLogicOp
}; // size 0x04

/*
 * Material
 *
 * The texturing and colouring state a pane draws with. Its textures, texture
 * coordinate generators, TEV stages and the other GX settings live in one block
 * (mpGXMem) laid out in the order of the Get...Ary accessors, sized by mGXMemCap.
 */
class Material {
public:
    Material();
    Material(const res::Material* pRes, const ResBlockSet& resBlockSet);
    virtual ~Material();

    virtual bool SetupGX(bool bModVtxCol, u8 alpha);
    virtual void BindAnimation(AnimTransform* pAnimTrans);
    virtual void UnbindAnimation(AnimTransform* pAnimTrans);
    virtual void UnbindAllAnimation();
    virtual void Animate();
    virtual AnimationLink* FindAnimationLink(AnimTransform* pAnimTrans);
    virtual void SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable);

    const char* GetName() const { return mName; }
    GXColorS10 GetTevColor(u32 idx) const { return mTevCols[idx]; }

    u8 GetTextureCap() const { return mGXMemCap.texMap; }
    u8 GetTexSRTCap() const { return mGXMemCap.texSRT; }
    u8 GetTexCoordGenCap() const { return mGXMemCap.texCoordGen; }
    u8 GetIndTexSRTCap() const { return mGXMemCap.indSRT; }
    bool IsTevSwapCap() const { return static_cast<bool>(mGXMemCap.tevSwap); }
    bool IsBlendModeCap() const { return static_cast<bool>(mGXMemCap.blendMode); }
    bool IsAlphaCompareCap() const { return static_cast<bool>(mGXMemCap.alpComp); }
    bool IsMatColorCap() const { return static_cast<bool>(mGXMemCap.matCol); }
    bool IsChanCtrlCap() const { return static_cast<bool>(mGXMemCap.chanCtrl); }

    u8 GetTextureNum() const { return mGXMemNum.texMap; }

    bool IsUserAllocated() const { return mbUserAllocated; }

    void SetName(const char* name);

    GXTexObj* GetTexMapAry();
    const GXTexObj* GetTexMapAry() const;
    TexSRT* GetTexSRTAry();
    const TexSRT* GetTexSRTAry() const;
    TexCoordGen* GetTexCoordGenAry();
    const TexCoordGen* GetTexCoordGenAry() const;
    ChanCtrl* GetChanCtrlAry();
    const ChanCtrl* GetChanCtrlAry() const;
    ut::Color* GetMatColAry();
    const ut::Color* GetMatColAry() const;
    TevSwapMode* GetTevSwapAry();
    const TevSwapMode* GetTevSwapAry() const;
    AlphaCompare* GetAlphaComparePtr();
    const AlphaCompare* GetAlphaComparePtr() const;
    BlendMode* GetBlendModePtr();
    const BlendMode* GetBlendModePtr() const;
    IndirectStage* GetIndirectStageAry();
    const IndirectStage* GetIndirectStageAry() const;
    TexSRT* GetIndTexSRTAry();
    const TexSRT* GetIndTexSRTAry() const;
    TevStage* GetTevStageAry();
    const TevStage* GetTevStageAry() const;

    void GetTexture(GXTexObj* pTexObj, u8 texMapIdx) const;
    s16 GetColorElement(u32 colorType) const;

    void SetTextureNum(u8 num);
    void SetTexCoordGenNum(u8 num);
    void SetIndStageNum(u8 num);
    void SetTevStageNum(u8 num);

    void SetTexture(u8 texMapIdx, TPLPalette* pTplRes);
    void SetTexture(u8 texMapIdx, const GXTexObj& texObj);
    void SetTextureNoWrap(u8 texMapIdx, TPLPalette* pTplRes);
    void SetTextureNoWrap(u8 texMapIdx, const GXTexObj& texObj);
    void SetColorElement(u32 colorType, s16 value);

    void SetTexCoordGen(u32 idx, TexCoordGen value) { GetTexCoordGenAry()[idx] = value; }

    void SetTexSRTElement(u32 texSRTIdx, u32 eleIdx, f32 value) {
        f32* srtAry = reinterpret_cast<f32*>(&GetTexSRTAry()[texSRTIdx]);

        srtAry[eleIdx] = value;
    }

    void SetIndTexSRTElement(u32 texSRTIdx, u32 eleIdx, f32 value) {
        f32* srtAry = reinterpret_cast<f32*>(&GetIndTexSRTAry()[texSRTIdx]);

        srtAry[eleIdx] = value;
    }

    void Init();
    void InitBitGXNums(detail::BitGXNums* ptr);

    void ReserveGXMem(u8 texMapNum, u8 texSRTNum, u8 texCoordGenNum, u8 tevStageNum, bool allocTevSwap,
                      u8 indStageNum, u8 indSRTNum, bool allocChanCtrl, bool allocMatCol, bool allocAlpComp,
                      bool allocBlendMode);

    void AddAnimationLink(AnimationLink* pAnimationLink);

private:
    char mName[20];                    // 0x04
    AnimationLink::LinkList mAnimList; // 0x18
    GXColorS10 mTevCols[3];            // 0x24
    ut::Color mTevKCols[4];            // 0x3C
    detail::BitGXNums mGXMemCap;       // 0x4C
    detail::BitGXNums mGXMemNum;       // 0x50
    bool mbUserAllocated;              // 0x54
    void* mpGXMem;                     // 0x58
}; // size 0x5C

namespace detail {

/*
 * GetTextureSize
 *
 * Width and height of texture `texMapIdx` of the material (zero when it has no such texture).
 */
Size GetTextureSize(Material* pMaterial, u8 texMapIdx);

} // namespace detail

} // namespace lyt
} // namespace nw4hbm

#endif
