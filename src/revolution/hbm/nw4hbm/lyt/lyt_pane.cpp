// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_pane.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_pane.cpp
#include "nw4hbm/lyt/pane.h"

#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/drawInfo.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/material.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/constants.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/LinkList.h"
#include "nw4hbm/ut/Rect.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

#include <revolution/gx.h>
#include <revolution/mtx.h>

#include <string.h>

namespace {

using namespace nw4hbm;

// Mirrors a matrix about the X axis, for layouts drawn with Y pointing up.
void ReverseYAxis(math::MTX34* pMtx) {
    pMtx->_01 = -pMtx->_01;
    pMtx->_11 = -pMtx->_11;
    pMtx->_21 = -pMtx->_21;
}

} // namespace

namespace nw4hbm {
namespace lyt {

const ut::detail::RuntimeTypeInfo Pane::typeInfo(NULL);

namespace detail {

/*
 * PaneBase::PaneBase
 */
PaneBase::PaneBase() {} // no copy in retail (inlined)

/*
 * PaneBase::~PaneBase
 */
PaneBase::~PaneBase() {} // no copy in retail (inlined)

} // namespace detail

/*
 * Pane::Pane
 *
 * Builds the pane from its data block.
 */
Pane::Pane(const res::Pane* pBlock) {
    Init();

    mBasePosition = pBlock->basePosition;
    SetName(pBlock->name);
    SetUserData(pBlock->userData);
    mTranslate = pBlock->translate;
    mRotate = pBlock->rotate;
    mScale = pBlock->scale;
    mSize = pBlock->size;
    mAlpha = pBlock->alpha;
    mGlbAlpha = mAlpha;
    mFlag = pBlock->flag;
} // 0x803C70E0

/*
 * Pane::Init
 */
void Pane::Init() {
    mpParent = NULL;
    mpMaterial = NULL;
    mbUserAllocated = false;
} // no copy in retail (inlined)

/*
 * Pane::~Pane
 *
 * Destroys the children and the material that the layout allocated; user-allocated ones are
 * only unlinked.
 */
Pane::~Pane() {
    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mChildList) {
        __typeof__(it) currIt = it++;

        mChildList.Erase(currIt);

        if (!currIt->IsUserAllocated()) {
            currIt->~Pane();
            Layout::FreeMemory(&(*currIt));
        }
    }

    UnbindAnimationSelf(NULL);

    if (mpMaterial && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
    }
} // 0x803C7200

/*
 * Pane::SetName
 */
void Pane::SetName(const char* name) {
    strncpy(mName, name, sizeof(mName));
} // no copy in retail (inlined)

/*
 * Pane::SetUserData
 */
void Pane::SetUserData(const char* userData) {
    strncpy(mUserData, userData, sizeof(mUserData));
} // no copy in retail (inlined)

/*
 * Pane::AppendChild
 *
 * Adds `pChild` at the end of the child list.
 */
void Pane::AppendChild(Pane* pChild) {
    InsertChild(mChildList.GetEndIter(), pChild);
} // 0x803C7340

/*
 * Pane::InsertChild
 */
void Pane::InsertChild(LinkList::Iterator next, Pane* pChild) {
    mChildList.Insert(next, pChild);
    pChild->mpParent = this;
} // no copy in retail (inlined)

/*
 * Pane::GetPaneRect
 *
 * The pane's rectangle in its own coordinates; top and bottom are negated for a
 * view whose Y axis points up.
 */
ut::Rect Pane::GetPaneRect(const DrawInfo& drawInfo) const {
    ut::Rect ret;
    math::VEC2 basePt = GetVtxPos();

    ret.left = basePt.x;
    ret.top = basePt.y;
    ret.right = ret.left + mSize.width;
    ret.bottom = ret.top + mSize.height;

    if (drawInfo.IsYAxisUp()) {
        ret.top = -ret.top;
        ret.bottom = -ret.bottom;
    }

    return ret;
} // 0x803C7390

/*
 * Pane::GetVtxColor
 *
 * A plain pane has no vertex colours: white.
 */
ut::Color Pane::GetVtxColor(u32) const {
    return ut::Color::WHITE;
} // 0x803C7460

/*
 * Pane::SetVtxColor
 */
void Pane::SetVtxColor(u32, ut::Color) {} // 0x803C7470

/*
 * Pane::GetColorElement
 *
 * Element 16 is the pane's alpha; the others are vertex colour elements.
 */
u8 Pane::GetColorElement(u32 idx) const {
    switch (idx) {
    case 16:
        return mAlpha;

    default:
        return GetVtxColorElement(idx);
    }
} // 0x803C7480

/*
 * Pane::SetColorElement
 */
void Pane::SetColorElement(u32 idx, u8 value) {
    switch (idx) {
    case 16:
        mAlpha = value;
        break;

    default:
        SetVtxColorElement(idx, value);
        break;
    }
} // 0x803C74B0

/*
 * Pane::GetVtxColorElement
 */
u8 Pane::GetVtxColorElement(u32) const {
    return 0xFF;
} // 0x803C74E0

/*
 * Pane::SetVtxColorElement
 */
void Pane::SetVtxColorElement(u32, u8) {} // 0x803C74F0

/*
 * Pane::FindPaneByName
 *
 * Looks for the pane named `findName` here and, when `bRecursive`, among the descendants.
 */
Pane* Pane::FindPaneByName(const char* findName, bool bRecursive) {
    if (detail::EqualsPaneName(mName, findName)) {
        return this;
    }

    if (bRecursive) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            if (Pane* pPane = it->FindPaneByName(findName, true)) {
                return pPane;
            }
        }
    }

    return NULL;
} // 0x803C7500

/*
 * Pane::FindMaterialByName
 *
 * Looks for the material named `findName` on this pane and, when `bRecursive`, on the
 * descendants.
 */
Material* Pane::FindMaterialByName(const char* findName, bool bRecursive) {
    if (mpMaterial && detail::EqualsMaterialName(mpMaterial->GetName(), findName)) {
        return mpMaterial;
    }

    if (bRecursive) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            if (Material* pMat = it->FindMaterialByName(findName, true)) {
                return pMat;
            }
        }
    }

    return NULL;
} // 0x803C75B0

/*
 * Pane::CalculateMtx
 *
 * Builds the local matrix from scale, rotation and translation, concatenates it with the
 * parent's global matrix, and does the same for the children. A pane that fades its
 * children (BIT_INFLUENCED_ALPHA) scales the draw info's global alpha for their pass.
 */
void Pane::CalculateMtx(const DrawInfo& drawInfo) {
    const f32 invAlpha = 1.0f / 255.0f;

    if (!detail::TestBit(mFlag, BIT_VISIBLE) && !drawInfo.IsInvisiblePaneCalculateMtx()) {
        return;
    }

    // Scoped as in the SDK source (its debug info shows these lexical blocks).
    {
        math::MTX34 mtx1;
        math::MTX34 mtx2;
        math::MTX34 rotateMtx;

        // Scoped as in the SDK source.
        {
            math::VEC2 scale(mScale);

            if (drawInfo.IsLocationAdjust() && detail::TestBit(mFlag, BIT_LOCATION_ADJUST)) {
                scale.x *= drawInfo.GetLocationAdjustScale().x;
                scale.y *= drawInfo.GetLocationAdjustScale().y;
            }

            MTXScale(mtx2, scale.x, scale.y, 1.0f);
        }

        MTXRotRad(rotateMtx, 'x', mRotate.x * math::convert::Deg2Rad);
        MTXConcat(rotateMtx, mtx2, mtx1);

        MTXRotRad(rotateMtx, 'y', mRotate.y * math::convert::Deg2Rad);
        MTXConcat(rotateMtx, mtx1, mtx2);

        MTXRotRad(rotateMtx, 'z', mRotate.z * math::convert::Deg2Rad);
        MTXConcat(rotateMtx, mtx2, mtx1);

        MTXTransApply(mtx1, mMtx, mTranslate.x, mTranslate.y, mTranslate.z);
    }

    if (mpParent) {
        math::MTX34Mult(&mGlbMtx, &mpParent->mGlbMtx, &mMtx);
    } else if (drawInfo.IsMultipleViewMtxOnDraw()) {
        mGlbMtx = mMtx;
    } else {
        math::MTX34Mult(&mGlbMtx, &drawInfo.GetViewMtx(), &mMtx);
    }

    if (drawInfo.IsInfluencedAlpha() && mpParent) {
        mGlbAlpha = mAlpha * drawInfo.GetGlobalAlpha();
    } else {
        mGlbAlpha = mAlpha;
    }

    f32 globalAlpha = drawInfo.GetGlobalAlpha();
    bool influencedAlpha = drawInfo.IsInfluencedAlpha();

    bool bModDrawInfo = detail::TestBit(mFlag, BIT_INFLUENCED_ALPHA) && mAlpha != 0xFF;

    if (bModDrawInfo) {
        DrawInfo& mutableDrawInfo = const_cast<DrawInfo&>(drawInfo);

        mutableDrawInfo.SetGlobalAlpha(globalAlpha * mAlpha * invAlpha);
        mutableDrawInfo.SetInfluencedAlpha(true);
    }

    CalculateMtxChild(drawInfo);

    if (bModDrawInfo) {
        DrawInfo& mutableDrawInfo = const_cast<DrawInfo&>(drawInfo);

        mutableDrawInfo.SetGlobalAlpha(globalAlpha);
        mutableDrawInfo.SetInfluencedAlpha(influencedAlpha);
    }
} // 0x803C7670

/*
 * Pane::CalculateMtxChild
 */
void Pane::CalculateMtxChild(const DrawInfo& drawInfo) {
    NW4HBM_RANGE_FOR(it, mChildList) {
        it->CalculateMtx(drawInfo);
    }
} // no copy in retail (inlined)

/*
 * Pane::Draw
 */
void Pane::Draw(const DrawInfo& drawInfo) {
    if (detail::TestBit(mFlag, BIT_VISIBLE)) {
        DrawSelf(drawInfo);

        NW4HBM_RANGE_FOR(it, mChildList) {
            it->Draw(drawInfo);
        }
    }
} // 0x803C7970

/*
 * Pane::DrawSelf
 *
 * Draws nothing, except that in debug draw mode a child pane outlines its rectangle in
 * green.
 */
void Pane::DrawSelf(const DrawInfo& drawInfo) {
    if (mpParent && drawInfo.IsDebugDrawMode()) {
        LoadMtx(drawInfo);
        detail::DrawLine(GetVtxPos(), mSize, 0x00FF00FF); // green (RGBA)
    }
} // 0x803C7A00

/*
 * Pane::Animate
 */
void Pane::Animate(u32 option) {
    AnimateSelf(option);

    if (detail::TestBit(mFlag, BIT_VISIBLE) || !(option & 1)) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            it->Animate(option);
        }
    }
} // 0x803C7A80

/*
 * Pane::AnimateSelf
 *
 * Applies every enabled animation bound to the pane, then animates its material (an
 * invisible pane's material only when `option` bit 0 is clear).
 */
void Pane::AnimateSelf(u32 option) {
    NW4HBM_RANGE_FOR(it, mAnimList) {
        if (it->IsEnable()) {
            AnimTransform* animTrans = it->GetAnimTransform();

            animTrans->Animate(it->GetIndex(), this);
        }
    }

    if ((detail::TestBit(mFlag, BIT_VISIBLE) || !(option & 1)) && mpMaterial) {
        mpMaterial->Animate();
    }
} // 0x803C7B10

/*
 * Pane::BindAnimation
 */
void Pane::BindAnimation(AnimTransform* pAnimTrans, bool bRecursive) {
    pAnimTrans->Bind(this, bRecursive);
} // 0x803C7BD0

/*
 * Pane::UnbindAnimation
 */
void Pane::UnbindAnimation(AnimTransform* pAnimTrans, bool bRecursive) {
    UnbindAnimationSelf(pAnimTrans);

    if (bRecursive) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            it->UnbindAnimation(pAnimTrans, bRecursive);
        }
    }
} // 0x803C7BF0

/*
 * Pane::UnbindAllAnimation
 */
void Pane::UnbindAllAnimation(bool bRecursive) {
    UnbindAnimation(NULL, bRecursive);
} // 0x803C7C80

/*
 * Pane::UnbindAnimationSelf
 *
 * Unlinks `pAnimTrans` from the pane and its material; NULL unlinks every animation.
 */
void Pane::UnbindAnimationSelf(AnimTransform* pAnimTrans) {
    if (mpMaterial) {
        mpMaterial->UnbindAnimation(pAnimTrans);
    }

    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mAnimList) {
        __typeof__(it) currIt = it++;

        if (pAnimTrans == NULL || currIt->GetAnimTransform() == pAnimTrans) {
            mAnimList.Erase(currIt);
            currIt->Reset();
        }
    }
} // 0x803C7CA0

/*
 * Pane::AddAnimationLink
 */
void Pane::AddAnimationLink(AnimationLink* pAnimationLink) {
    mAnimList.PushBack(pAnimationLink);
} // 0x803C7D40

/*
 * Pane::FindAnimationLink
 *
 * The link joining `pAnimTrans` to the pane, or failing that to its material.
 */
AnimationLink* Pane::FindAnimationLink(AnimTransform* pAnimTrans) {
    if (AnimationLink* ret = detail::FindAnimationLink(&mAnimList, pAnimTrans)) {
        return ret;
    }

    if (mpMaterial) {
        if (AnimationLink* ret = mpMaterial->FindAnimationLink(pAnimTrans)) {
            return ret;
        }
    }

    return NULL;
} // 0x803C7D80

/*
 * Pane::SetAnimationEnable
 */
void Pane::SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable, bool bRecursive) {
    if (AnimationLink* pAnimLink = detail::FindAnimationLink(&mAnimList, pAnimTrans)) {
        pAnimLink->SetEnable(bEnable);
    }

    if (mpMaterial) {
        mpMaterial->SetAnimationEnable(pAnimTrans, bEnable);
    }

    if (bRecursive) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            it->SetAnimationEnable(pAnimTrans, bEnable, bRecursive);
        }
    }
} // 0x803C7E00

/*
 * Pane::LoadMtx
 *
 * Loads the pane's global matrix (times the view matrix when that is applied per draw, and
 * mirrored for a Y-up view) as the current GX position matrix.
 */
void Pane::LoadMtx(const DrawInfo& drawInfo) {
    math::MTX34 mtx;
    MtxPtr mtxPtr = NULL;

    if (drawInfo.IsMultipleViewMtxOnDraw()) {
        math::MTX34Mult(&mtx, &drawInfo.GetViewMtx(), &mGlbMtx);

        if (drawInfo.IsYAxisUp()) {
            ReverseYAxis(&mtx);
        }

        mtxPtr = mtx;
    } else if (drawInfo.IsYAxisUp()) {
        math::MTX34Copy(&mtx, &mGlbMtx);
        ReverseYAxis(&mtx);
        mtxPtr = mtx;
    } else {
        mtxPtr = mGlbMtx;
    }

    GXLoadPosMtxImm(mtxPtr, 0);
    GXSetCurrentMtx(0);
} // 0x803C7EC0

/*
 * Pane::GetVtxPos
 *
 * Offset of the pane's top-left corner from its origin, which sits at the base position
 * (left, centre or right by mBasePosition % 3; top, centre or bottom by mBasePosition / 3).
 */
math::VEC2 Pane::GetVtxPos() const {
    math::VEC2 basePt(0.0f, 0.0f);

    switch (mBasePosition % 3) {
    default:
    case 0:
        basePt.x = 0.0f;
        break;

    case 1:
        basePt.x = -mSize.width / 2.0f;
        break;

    case 2:
        basePt.x = -mSize.width;
        break;
    }

    switch (mBasePosition / 3) {
    default:
    case 0:
        basePt.y = 0.0f;
        break;

    case 1:
        basePt.y = -mSize.height / 2.0f;
        break;

    case 2:
        basePt.y = -mSize.height;
        break;
    }

    return basePt;
} // 0x803C7FD0

/*
 * Pane::GetMaterial
 */
Material* Pane::GetMaterial() const {
    return mpMaterial;
} // 0x803C80B0

} // namespace lyt
} // namespace nw4hbm
