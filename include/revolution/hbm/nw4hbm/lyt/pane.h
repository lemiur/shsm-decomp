#ifndef NW4HBM_LYT_PANE_H
#define NW4HBM_LYT_PANE_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/pane.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_pane.h

#include <revolution/types.h>
#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/LinkList.h"
#include "nw4hbm/ut/Rect.h"

namespace nw4hbm {
namespace ut {
namespace detail {
class RuntimeTypeInfo;
} // namespace detail
} // namespace ut

namespace lyt {
class DrawInfo;
class Material;

class Pane;

namespace res {

// data-block kinds, the four characters as a big-endian word
static const u32 SIGNATURE_PANE_BLOCK = 'pan1';
static const u32 SIGNATURE_PANE_START_BLOCK = 'pas1';
static const u32 SIGNATURE_PANE_END_BLOCK = 'pae1';

/*
 * res::Pane
 *
 * Layout-file data block of a pane ("pan1").
 */
struct Pane {
    DataBlockHeader blockHeader; // 0x00
    u8 flag;                     // 0x08
    u8 basePosition;             // 0x09
    u8 alpha;                    // 0x0A
    u8 padding;                  // 0x0B // pad: SDK res::Pane.padding
    char name[16];               // 0x0C
    char userData[8];            // 0x1C
    math::VEC3 translate;        // 0x24
    math::VEC3 rotate;           // 0x30
    math::VEC2 scale;            // 0x3C
    Size size;                   // 0x44
}; // size 0x4C

} // namespace res

namespace detail {

/*
 * PaneLink
 *
 * List element that refers to a pane, as held by a Group.
 */
struct PaneLink {
    typedef ut::LinkList<PaneLink, 0> LinkList;

    ut::LinkListNode mLink; // 0x00
    Pane* mTarget;          // 0x08
}; // size 0x0C

/*
 * PaneBase
 *
 * Root of the pane hierarchy: the sibling list node.
 */
class PaneBase {
public:
    typedef ut::LinkList<PaneBase, 4> LinkList;

    PaneBase();
    virtual ~PaneBase();

private:
    ut::LinkListNode mLink; // 0x04
}; // size 0x0C

} // namespace detail

/*
 * Pane
 *
 * A node of the layout tree: position, rotation, scale, size and colour, with a
 * list of child panes and of the animations bound to it.
 */
class Pane : public detail::PaneBase {
public:
    typedef ut::LinkList<Pane, 4> LinkList;

    // Bits of mFlag.
    enum {
        BIT_VISIBLE,
        BIT_INFLUENCED_ALPHA,
        BIT_LOCATION_ADJUST,
    };

    Pane();
    Pane(const res::Pane* pBlock);
    virtual ~Pane();

    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const { return &typeInfo; }

    virtual void CalculateMtx(const DrawInfo& drawInfo);
    virtual void Draw(const DrawInfo& drawInfo);
    virtual void DrawSelf(const DrawInfo& drawInfo);
    virtual void Animate(u32 option);
    virtual void AnimateSelf(u32 option);
    virtual ut::Color GetVtxColor(u32 idx) const;
    virtual void SetVtxColor(u32 idx, ut::Color value);
    virtual u8 GetColorElement(u32 idx) const;
    virtual void SetColorElement(u32 idx, u8 value);
    virtual u8 GetVtxColorElement(u32 idx) const;
    virtual void SetVtxColorElement(u32 idx, u8 value);
    virtual Pane* FindPaneByName(const char* findName, bool bRecursive);
    virtual Material* FindMaterialByName(const char* findName, bool bRecursive);
    virtual void BindAnimation(AnimTransform* pAnimTrans, bool bRecursive);
    virtual void UnbindAnimation(AnimTransform* pAnimTrans, bool bRecursive);
    virtual void UnbindAllAnimation(bool bRecursive);
    virtual void UnbindAnimationSelf(AnimTransform* pAnimTrans);
    virtual AnimationLink* FindAnimationLink(AnimTransform* pAnimTrans);
    virtual void SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable, bool bRecursive);
    virtual Material* GetMaterial() const;
    virtual void LoadMtx(const DrawInfo& drawInfo);

    Pane* GetParent() const { return mpParent; }
    LinkList& GetChildList() { return mChildList; }
    const math::VEC3& GetTranslate() const { return mTranslate; }
    const Size& GetSize() const { return mSize; }
    const math::MTX34& GetGlobalMtx() const { return mGlbMtx; }
    bool IsVisible() const { return detail::TestBit(mFlag, BIT_VISIBLE); }
    bool IsInfluencedAlpha() const { return detail::TestBit(mFlag, BIT_INFLUENCED_ALPHA); }
    bool IsLocationAdjust() const { return detail::TestBit(mFlag, BIT_LOCATION_ADJUST); }
    const char* GetName() const { return mName; }
    bool IsUserAllocated() const { return mbUserAllocated; }

    void SetSRTElement(u32 idx, f32 value) {
        f32* srtAry = reinterpret_cast<f32*>(&mTranslate);

        srtAry[idx] = value;
    }

    void SetRotate(const math::VEC3& value) { mRotate = value; }
    void SetScale(const math::VEC2& value) { mScale = value; }
    void SetTranslate(const math::VEC3& value) { mTranslate = value; }
    void SetTranslate(const math::VEC2& value) {
        SetTranslate(math::VEC3(value.x, value.y, 0.0f));
    }
    void SetSize(const Size& value) { mSize = value; }
    void SetVisible(bool bVisible) { detail::SetBit(&mFlag, BIT_VISIBLE, bVisible); }
    void SetAlpha(u8 alpha) { mAlpha = alpha; }

    ut::Rect GetPaneRect(const DrawInfo& drawInfo) const;
    math::VEC2 GetVtxPos() const;

    void SetName(const char* name);
    void SetUserData(const char* userData);

    void Init();

    void PrependChild(Pane* pChild);
    void InsertChild(Pane* pNext, Pane* pChild);
    void InsertChild(LinkList::Iterator next, Pane* pChild);
    void AppendChild(Pane* pChild);
    void RemoveChild(Pane* pChild);

    void CalculateMtxChild(const DrawInfo& drawInfo);

    void AddAnimationLink(AnimationLink* pAnimationLink);

    static const ut::detail::RuntimeTypeInfo typeInfo;

protected:
    Pane* mpParent;                    // 0x0C
    LinkList mChildList;               // 0x10
    AnimationLink::LinkList mAnimList; // 0x1C
    Material* mpMaterial;              // 0x28
    math::VEC3 mTranslate;             // 0x2C
    math::VEC3 mRotate;                // 0x38
    math::VEC2 mScale;                 // 0x44
    Size mSize;                        // 0x4C
    math::MTX34 mMtx;                  // 0x54
    math::MTX34 mGlbMtx;               // 0x84
    char mName[16];                    // 0xB4
    char mUserData[8];                 // 0xC4
    u8 mBasePosition;                  // 0xCC
    u8 mAlpha;                         // 0xCD
    u8 mGlbAlpha;                      // 0xCE
    u8 mFlag;                          // 0xCF BIT_* flags
    bool mbUserAllocated;              // 0xD0
}; // size 0xD4

/* A pane's child list, as the layout code names it (the reference's PaneList). */
typedef ut::LinkList<Pane, 4> PaneList;

} // namespace lyt
} // namespace nw4hbm

#endif
