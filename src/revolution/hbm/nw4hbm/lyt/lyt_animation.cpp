// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_animation.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_animation.cpp
#include "nw4hbm/lyt/animation.h"

#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/material.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/lyt/resourceAccessor.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/ut/inlines.h"
#include "nw4hbm/ut/LinkList.h"

#include <new>
#include <string.h>

#include <revolution/os/OSFastCast.h>
#include <revolution/tpl.h>

namespace {

using namespace nw4hbm;
using namespace nw4hbm::lyt;

/*
 * RIsSame
 *
 * Whether `a` and `b` differ by less than `tolerance`.
 */
inline bool RIsSame(const f32 a, const f32 b, const f32 tolerance) {
    const f32 c = a - b;

    return -tolerance < c && c < tolerance;
} // no copy in retail (inlined)

/*
 * GetStepCurveValue
 *
 * Value of the step curve `keyArray` at `frame`: the value of the last key at
 * or before it.
 */
u16 GetStepCurveValue(f32 frame, const res::StepKey* keyArray, u32 keySize) {
    if (keySize == 1 || frame <= keyArray[0].frame) {
        return keyArray[0].value;
    }

    if (frame >= keyArray[keySize - 1].frame) {
        return keyArray[keySize - 1].value;
    }

    int ikeyL = 0;
    int ikeyR = keySize - 1;

    while (ikeyL != ikeyR - 1 && ikeyL != ikeyR) {
        int ikeyCenter = (ikeyL + ikeyR) / 2;
        const res::StepKey& centerKey = keyArray[ikeyCenter];

        if (frame < centerKey.frame) {
            ikeyR = ikeyCenter;
        } else {
            ikeyL = ikeyCenter;
        }
    }

    if (RIsSame(frame, keyArray[ikeyR].frame, 1e-3f)) {
        return keyArray[ikeyR].value;
    } else {
        return keyArray[ikeyL].value;
    }
} // 0x803C0E60

/*
 * GetHermiteCurveValue
 *
 * Value of the Hermite curve `keyArray` at `frame`. Two keys on the same frame
 * make a step: at that frame the later key's value is taken.
 */
f32 GetHermiteCurveValue(f32 frame, const res::HermiteKey* keyArray, u32 keySize) {
    if (keySize == 1 || frame <= keyArray[0].frame) {
        return keyArray[0].value;
    }

    if (frame >= keyArray[keySize - 1].frame) {
        return keyArray[keySize - 1].value;
    }

    int ikeyL = 0;
    int ikeyR = keySize - 1;

    while (ikeyL != ikeyR - 1 && ikeyL != ikeyR) {
        int ikeyCenter = (ikeyL + ikeyR) / 2;

        if (frame <= keyArray[ikeyCenter].frame) {
            ikeyR = ikeyCenter;
        } else {
            ikeyL = ikeyCenter;
        }
    }

    const res::HermiteKey& key0 = keyArray[ikeyL];
    const res::HermiteKey& key1 = keyArray[ikeyR];

    if (RIsSame(frame, key1.frame, 1e-3f)) {
        if (ikeyR < keySize - 1 && keyArray[ikeyR + 1].frame == key1.frame) {
            return keyArray[ikeyR + 1].value;
        } else {
            return key1.value;
        }
    }

    f32 t1 = frame - key0.frame;
    f32 t2 = 1.0f / (key1.frame - key0.frame);
    f32 v0 = key0.value;
    f32 v1 = key1.value;
    f32 s0 = key0.slope;
    f32 s1 = key1.slope;

    f32 t1t1t2 = t1 * t1 * t2;
    f32 t1t1t2t2 = t1t1t2 * t2;
    f32 t1t1t1t2t2 = t1 * t1t1t2t2;
    f32 t1t1t1t2t2t2 = t1t1t1t2t2 * t2;

    // The cubic Hermite basis in t = t1 * t2, the position between the keys, with
    // the slopes scaled by the key distance 1 / t2.
    return v0 * (2.0f * t1t1t1t2t2t2 - 3.0f * t1t1t2t2 + 1.0f) + v1 * (-2.0f * t1t1t1t2t2t2 + 3.0f * t1t1t2t2) +
           s0 * (t1t1t1t2t2 - 2.0f * t1t1t2 + t1) + s1 * (t1t1t1t2t2 - t1t1t2);
} // 0x803C0F50

/*
 * AnimatePainSRT
 *
 * Applies the pane translate/rotate/scale/size curves at `frame`.
 */
void AnimatePainSRT(Pane* pPane, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets, f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        const res::HermiteKey* keys = detail::ConvertOffsToPtr<res::HermiteKey>(pAnimTarget, pAnimTarget->keysOffset);

        pPane->SetSRTElement(pAnimTarget->target, GetHermiteCurveValue(frame, keys, pAnimTarget->keyNum));
    }
} // no copy in retail (inlined)

/*
 * AnimateVisibility
 *
 * Applies the pane visibility curves at `frame`.
 */
void AnimateVisibility(Pane* pPane, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets, f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        const res::StepKey* keys = detail::ConvertOffsToPtr<res::StepKey>(pAnimTarget, pAnimTarget->keysOffset);

        pPane->SetVisible(GetStepCurveValue(frame, keys, pAnimTarget->keyNum) != 0);
    }
} // no copy in retail (inlined)

/*
 * AnimateVertexColor
 *
 * Applies the pane vertex-colour curves at `frame`, rounded to u8.
 */
void AnimateVertexColor(Pane* pPane, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets, f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        const res::HermiteKey* keys = detail::ConvertOffsToPtr<res::HermiteKey>(pAnimTarget, pAnimTarget->keysOffset);

        f32 value = GetHermiteCurveValue(frame, keys, pAnimTarget->keyNum);
        value += 0.5f;

        u8 u8Val;
        OSf32tou8(&value, &u8Val);

        pPane->SetColorElement(pAnimTarget->target, u8Val);
    }
} // no copy in retail (inlined)

/*
 * AnimateMaterialColor
 *
 * Applies the material colour curves at `frame`, rounded to s16 and clamped to
 * the TEV colour range.
 */
void AnimateMaterialColor(Material* pMaterial, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets,
                          f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        const res::HermiteKey* keys = detail::ConvertOffsToPtr<res::HermiteKey>(pAnimTarget, pAnimTarget->keysOffset);

        f32 value = GetHermiteCurveValue(frame, keys, pAnimTarget->keyNum);
        value += 0.5f;

        s16 s16Val;
        OSf32tos16(&value, &s16Val);
        s16Val = ut::Min<s16>(ut::Max<s16>(s16Val, -1024), 1023);

        pMaterial->SetColorElement(pAnimTarget->target, s16Val);
    }
} // no copy in retail (inlined)

/*
 * AnimateTextureSRT
 *
 * Applies the texture SRT curves at `frame` to the entries the material has.
 */
void AnimateTextureSRT(Material* pMaterial, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets,
                       f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        if (pAnimTarget->id < pMaterial->GetTexSRTCap()) {
            const res::HermiteKey* keys =
                detail::ConvertOffsToPtr<res::HermiteKey>(pAnimTarget, pAnimTarget->keysOffset);

            pMaterial->SetTexSRTElement(pAnimTarget->id, pAnimTarget->target,
                                        GetHermiteCurveValue(frame, keys, pAnimTarget->keyNum));
        }
    }
} // no copy in retail (inlined)

/*
 * AnimateTexturePattern
 *
 * Applies the texture pattern curves at `frame`: each selects one of the
 * animation's texture files for a texture map the material has.
 */
void AnimateTexturePattern(Material* pMaterial, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets,
                           f32 frame, void** tpls) {
    for (int j = 0; j < pAnimInfo->num; ++j) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[j]);

        if (pAnimTarget->id < pMaterial->GetTextureNum() && !pAnimTarget->target) {
            const res::StepKey* keys = detail::ConvertOffsToPtr<res::StepKey>(pAnimTarget, pAnimTarget->keysOffset);

            u16 fileIdx = GetStepCurveValue(frame, keys, pAnimTarget->keyNum);

            pMaterial->SetTextureNoWrap(pAnimTarget->id, static_cast<TPLPalette*>(tpls[fileIdx]));
        }
    }
} // no copy in retail (inlined)

/*
 * AnimateIndTexSRT
 *
 * Applies the indirect texture SRT curves at `frame` to the entries the
 * material has.
 */
void AnimateIndTexSRT(Material* pMaterial, const res::AnimationInfo* pAnimInfo, const u32* animTargetOffsets,
                      f32 frame) {
    for (int i = 0; i < pAnimInfo->num; ++i) {
        const res::AnimationTarget* pAnimTarget =
            detail::ConvertOffsToPtr<res::AnimationTarget>(pAnimInfo, animTargetOffsets[i]);

        if (pAnimTarget->id < pMaterial->GetIndTexSRTCap()) {
            const res::HermiteKey* keys =
                detail::ConvertOffsToPtr<res::HermiteKey>(pAnimTarget, pAnimTarget->keysOffset);

            pMaterial->SetIndTexSRTElement(pAnimTarget->id, pAnimTarget->target,
                                           GetHermiteCurveValue(frame, keys, pAnimTarget->keyNum));
        }
    }
} // no copy in retail (inlined)

} // namespace

namespace nw4hbm {
namespace lyt {

/*
 * AnimTransform::AnimTransform
 */
AnimTransform::AnimTransform() : mpRes(NULL), mFrame(0.0f) {
} // no copy in retail (inlined)

/*
 * AnimTransform::~AnimTransform
 */
AnimTransform::~AnimTransform() {
} // no copy in retail (inlined)

/*
 * AnimTransform::GetFrameSize
 */
u16 AnimTransform::GetFrameSize() const {
    return mpRes->frameSize;
} // 0x803C1120

/*
 * AnimTransformBasic::AnimTransformBasic
 */
AnimTransformBasic::AnimTransformBasic() : mpFileResAry(NULL), mAnimLinkAry(NULL), mAnimLinkNum(0) {
} // 0x803C1130

/*
 * AnimTransformBasic::~AnimTransformBasic
 */
AnimTransformBasic::~AnimTransformBasic() {
    if (mAnimLinkAry) {
        Layout::FreeMemory(mAnimLinkAry);
    }

    if (mpFileResAry) {
        Layout::FreeMemory(mpFileResAry);
    }
} // 0x803C1170

/*
 * AnimTransformBasic::SetResource
 *
 * Takes the animation block, loads the texture files its patterns name through
 * `pResAccessor` and allocates one link per animation content.
 */
void AnimTransformBasic::SetResource(const res::AnimationBlock* pRes, ResourceAccessor* pResAccessor) {
    mpRes = pRes;
    mpFileResAry = NULL;

    if (pRes->fileNum) {
        mpFileResAry = static_cast<void**>(Layout::AllocMemory(sizeof(*mpFileResAry) * pRes->fileNum));

        if (mpFileResAry) {
            const u32* fileNameOffsets = detail::ConvertOffsToPtr<u32>(mpRes, sizeof(*mpRes));

            for (int i = 0; i < mpRes->fileNum; ++i) {
                mpFileResAry[i] = pResAccessor->GetResource(ResourceAccessor::SIGNATURE_TEXTURE_IMAGE,
                                                            detail::GetStrTableStr(fileNameOffsets, i), NULL);
            }
        }
    }

    mAnimLinkAry = static_cast<AnimationLink*>(Layout::AllocMemory(sizeof(*mAnimLinkAry) * pRes->animContNum));

    if (mAnimLinkAry) {
        mAnimLinkNum = pRes->animContNum;

        memset(mAnimLinkAry, 0, sizeof(*mAnimLinkAry) * pRes->animContNum);

        for (u16 i = 0; i < pRes->animContNum; ++i) {
            new (&mAnimLinkAry[i]) AnimationLink;
        }
    }
} // 0x803C1200

/*
 * AnimTransformBasic::Bind
 *
 * Links each animation content to the pane or material of its name under
 * `pPane`.
 */
void AnimTransformBasic::Bind(Pane* pPane, bool bRecursive) {
    const u32* animContOffsets = detail::ConvertOffsToPtr<u32>(mpRes, mpRes->animContOffsetsOffset);

    for (u16 i = 0; i < mpRes->animContNum; ++i) {
        const res::AnimationContent* pAnimCont =
            detail::ConvertOffsToPtr<res::AnimationContent>(mpRes, animContOffsets[i]);

        if (pAnimCont->type == res::AnimationContent::ACType_Pane) {
            Pane* pFindPane = pPane->FindPaneByName(pAnimCont->name, bRecursive);

            if (pFindPane) {
                mAnimLinkAry[i].SetAnimTransform(this, i);
                pFindPane->AddAnimationLink(&mAnimLinkAry[i]);
            }
        } else {
            Material* pFindMat = pPane->FindMaterialByName(pAnimCont->name, bRecursive);

            if (pFindMat) {
                mAnimLinkAry[i].SetAnimTransform(this, i);
                pFindMat->AddAnimationLink(&mAnimLinkAry[i]);
            }
        }
    }
} // 0x803C1350

/*
 * AnimTransformBasic::Bind
 *
 * Links the material animation contents named after `pMaterial` to it.
 */
void AnimTransformBasic::Bind(Material* pMaterial) {
    const u32* animContOffsets = detail::ConvertOffsToPtr<u32>(mpRes, mpRes->animContOffsetsOffset);

    for (u16 i = 0; i < mpRes->animContNum; ++i) {
        const res::AnimationContent* pAnimCont =
            detail::ConvertOffsToPtr<res::AnimationContent>(mpRes, animContOffsets[i]);

        if (pAnimCont->type == res::AnimationContent::ACType_Material &&
            detail::EqualsMaterialName(pMaterial->GetName(), pAnimCont->name)) {
            mAnimLinkAry[i].SetAnimTransform(this, i);
            pMaterial->AddAnimationLink(&mAnimLinkAry[i]);
        }
    }
} // 0x803C1450

/*
 * AnimTransformBasic::Animate
 *
 * Applies animation content `idx` to `pPane` at the current frame.
 */
void AnimTransformBasic::Animate(u32 idx, Pane* pPane) {
    u32 animContOffsets = detail::ConvertOffsToPtr<u32>(mpRes, mpRes->animContOffsetsOffset)[idx];

    const res::AnimationContent* pAnimCont = detail::ConvertOffsToPtr<res::AnimationContent>(mpRes, animContOffsets);

    const u32* animInfoOffsets = detail::ConvertOffsToPtr<u32>(pAnimCont, sizeof(*pAnimCont));

    for (int i = 0; i < pAnimCont->num; ++i) {
        const res::AnimationInfo* pAnimInfo =
            detail::ConvertOffsToPtr<res::AnimationInfo>(pAnimCont, animInfoOffsets[i]);

        const u32* animTargetOffsets = detail::ConvertOffsToPtr<u32>(pAnimInfo, sizeof(*pAnimInfo));

        switch (pAnimInfo->kind) {
        case res::AnimationInfo::SIGNATURE_PANE_PAIN_SRT_INFO:
            AnimatePainSRT(pPane, pAnimInfo, animTargetOffsets, mFrame);
            break;

        case res::AnimationInfo::SIGNATURE_PANE_VISIBILITY_INFO:
            AnimateVisibility(pPane, pAnimInfo, animTargetOffsets, mFrame);
            break;

        case res::AnimationInfo::SIGNATURE_PANE_VERTEX_COLOR_INFO:
            AnimateVertexColor(pPane, pAnimInfo, animTargetOffsets, mFrame);
            break;
        }
    }
} // 0x803C1510

/*
 * AnimTransformBasic::Animate
 *
 * Applies animation content `idx` to `pMaterial` at the current frame.
 */
void AnimTransformBasic::Animate(u32 idx, Material* pMaterial) {
    u32 animContOffsets = detail::ConvertOffsToPtr<u32>(mpRes, mpRes->animContOffsetsOffset)[idx];

    const res::AnimationContent* pAnimCont = detail::ConvertOffsToPtr<res::AnimationContent>(mpRes, animContOffsets);

    const u32* animInfoOffsets = detail::ConvertOffsToPtr<u32>(pAnimCont, sizeof(*pAnimCont));

    for (int i = 0; i < pAnimCont->num; ++i) {
        const res::AnimationInfo* pAnimInfo =
            detail::ConvertOffsToPtr<res::AnimationInfo>(pAnimCont, animInfoOffsets[i]);

        const u32* animTargetOffsets = detail::ConvertOffsToPtr<u32>(pAnimInfo, sizeof(*pAnimInfo));

        switch (pAnimInfo->kind) {
        case res::AnimationInfo::SIGNATURE_MATERIAL_COLOR_INFO:
            AnimateMaterialColor(pMaterial, pAnimInfo, animTargetOffsets, mFrame);
            break;

        case res::AnimationInfo::SIGNATURE_MATERIAL_TEXTURE_SRT_INFO:
            AnimateTextureSRT(pMaterial, pAnimInfo, animTargetOffsets, mFrame);
            break;

        case res::AnimationInfo::SIGNATURE_MATERIAL_TEXTURE_PATTERN_INFO:
            if (mpFileResAry) {
                AnimateTexturePattern(pMaterial, pAnimInfo, animTargetOffsets, mFrame, mpFileResAry);
            }

            break;

        case res::AnimationInfo::SIGNATURE_MATERIAL_IND_TEX_SRT_INFO:
            AnimateIndTexSRT(pMaterial, pAnimInfo, animTargetOffsets, mFrame);
            break;
        }
    }
} // 0x803C16F0

namespace detail {

/*
 * FindAnimationLink
 *
 * Returns the link in `pAnimList` that joins `pAnimTrans`, or NULL.
 */
AnimationLink* FindAnimationLink(AnimationLink::LinkList* pAnimList, AnimTransform* pAnimTrans) {
    NW4HBM_RANGE_FOR(it, *pAnimList) {
        if (pAnimTrans == it->GetAnimTransform()) {
            return &(*it);
        }
    }

    return NULL;
} // 0x803C19D0

} // namespace detail

} // namespace lyt
} // namespace nw4hbm
