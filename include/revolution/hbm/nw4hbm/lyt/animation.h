#ifndef NW4HBM_LYT_ANIMATION_H
#define NW4HBM_LYT_ANIMATION_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/animation.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_animation.h

#include <revolution/types.h>
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/ut/LinkList.h"

namespace nw4hbm {
namespace lyt {
class Material;
class Pane;
class ResourceAccessor;

namespace res {

static const u32 SIGNATURE_ANIMATION = 'RLAN';         // animation file
static const u32 SIGNATURE_ANIM_INFO_BLOCK = 'pai1';   // AnimationBlock

/*
 * res::AnimationBlock
 *
 * Animation-file data block ("pai1"): the frame count and loop flag, the names
 * of the texture files the patterns use (a string table after the header) and
 * the offsets of the animation contents.
 */
struct AnimationBlock {
    DataBlockHeader blockHeader; // 0x00
    u16 frameSize;               // 0x08
    u8 loop;                     // 0x0A
    u8 padding1;                 // 0x0B // pad: SDK res::AnimationBlock.padding1
    u16 fileNum;                 // 0x0C
    u16 animContNum;             // 0x0E
    u32 animContOffsetsOffset;   // 0x10
}; // size 0x14

/*
 * res::AnimationContent
 *
 * The animations of one pane or material, named by `name`; the offsets of its
 * `num` animation infos follow the header.
 */
struct AnimationContent {
    enum {
        ACType_Pane,
        ACType_Material
    };

    static const u32 NAME_LENGTH = 20;

    char name[NAME_LENGTH]; // 0x00
    u8 num;                 // 0x14
    u8 type;                // 0x15 ACType_*
    u8 padding[2];          // 0x16 // pad: SDK res::AnimationContent.padding
}; // size 0x18

/*
 * res::AnimationInfo
 *
 * One kind of animation of a content; the offsets of its `num` targets follow
 * the header.
 */
struct AnimationInfo {
    static const u32 SIGNATURE_PANE_PAIN_SRT_INFO = 'RLPA';
    static const u32 SIGNATURE_PANE_VERTEX_COLOR_INFO = 'RLVC';
    static const u32 SIGNATURE_PANE_VISIBILITY_INFO = 'RLVI';
    static const u32 SIGNATURE_MATERIAL_IND_TEX_SRT_INFO = 'RLIM';
    static const u32 SIGNATURE_MATERIAL_COLOR_INFO = 'RLMC';
    static const u32 SIGNATURE_MATERIAL_TEXTURE_PATTERN_INFO = 'RLTP';
    static const u32 SIGNATURE_MATERIAL_TEXTURE_SRT_INFO = 'RLTS';

    u32 kind;      // 0x00 SIGNATURE_*_INFO
    u8 num;        // 0x04
    u8 padding[3]; // 0x05 // pad: SDK res::AnimationInfo.padding
}; // size 0x08

/*
 * res::AnimationTarget
 *
 * One animated element (`target`) of entry `id` of an animation info, and the
 * offset of its `keyNum` keys.
 */
struct AnimationTarget {
    u8 id;          // 0x00
    u8 target;      // 0x01
    u8 curveType;   // 0x02
    u8 padding1;    // 0x03 // pad: SDK res::AnimationTarget.padding1
    u16 keyNum;     // 0x04
    u8 padding2[2]; // 0x06 // pad: SDK res::AnimationTarget.padding2
    u32 keysOffset; // 0x08
}; // size 0x0C

/*
 * res::HermiteKey
 *
 * A key of a Hermite curve: its frame, value and slope.
 */
struct HermiteKey {
    f32 frame; // 0x00
    f32 value; // 0x04
    f32 slope; // 0x08
}; // size 0x0C

/*
 * res::StepKey
 *
 * A key of a step curve: the value holds from its frame to the next key's.
 */
struct StepKey {
    f32 frame;   // 0x00
    u16 value;   // 0x04
    u16 padding; // 0x06 // pad: SDK res::StepKey.padding
}; // size 0x08

} // namespace res

class AnimTransform;

/*
 * AnimationLink
 *
 * Joins a pane (or material) to one of an animation's contents; disabled links
 * are skipped when the animation is applied.
 */
class AnimationLink {
public:
    typedef ut::LinkList<AnimationLink, 0> LinkList;

    AnimationLink() : mbDisable(false) { Reset(); }

    AnimTransform* GetAnimTransform() const { return mAnimTrans; }
    u16 GetIndex() const { return mIdx; }
    bool IsEnable() const { return !mbDisable; }

    void SetAnimTransform(AnimTransform* animTrans, u16 idx) {
        mAnimTrans = animTrans;
        mIdx = idx;
    }

    void SetEnable(bool bEnable) { mbDisable = !bEnable; }

    void Reset() { SetAnimTransform(NULL, 0); }

private:
    ut::LinkListNode mLink;    // 0x00
    AnimTransform* mAnimTrans; // 0x08
    u16 mIdx;                  // 0x0C
    bool mbDisable;            // 0x0E
}; // size 0x10

/*
 * AnimTransform
 *
 * An animation resource bound to panes and materials: the current frame is
 * set by the owner and each target animates itself from it.
 */
class AnimTransform {
public:
    typedef ut::LinkList<AnimTransform, 4> LinkList;

    AnimTransform();
    virtual ~AnimTransform();

    virtual void SetResource(const res::AnimationBlock* pRes, ResourceAccessor* pResAccessor) = 0;
    virtual void Bind(Pane* pPane, bool bRecursive) = 0;
    virtual void Bind(Material* pMaterial) = 0;
    virtual void Animate(u32 idx, Pane* pPane) = 0;
    virtual void Animate(u32 idx, Material* pMaterial) = 0;

    f32 GetFrameMax() const { return static_cast<f32>(GetFrameSize()); }
    u16 GetFrameSize() const;

    void SetFrame(f32 frame) { mFrame = frame; }

    bool IsLoopData() const;

protected:
    ut::LinkListNode mLink;           // 0x04
    const res::AnimationBlock* mpRes; // 0x0C
    f32 mFrame;                       // 0x10
}; // size 0x14

/*
 * AnimTransformBasic
 *
 * The animation transform of the layout library: binds each animation content
 * to the pane or material of its name and evaluates its curves at the current
 * frame.
 */
class AnimTransformBasic : public AnimTransform {
public:
    AnimTransformBasic();
    virtual ~AnimTransformBasic();

    virtual void SetResource(const res::AnimationBlock* pRes, ResourceAccessor* pResAccessor);
    virtual void Bind(Pane* pPane, bool bRecursive);
    virtual void Bind(Material* pMaterial);
    virtual void Animate(u32 idx, Pane* pPane);
    virtual void Animate(u32 idx, Material* pMaterial);

private:
    void** mpFileResAry;          // 0x14 texture files of the patterns
    AnimationLink* mAnimLinkAry;  // 0x18 one link per animation content
    u16 mAnimLinkNum;             // 0x1C
}; // size 0x20

namespace detail {

/*
 * FindAnimationLink
 *
 * Returns the link in `pAnimList` that joins `pAnimTrans`, or NULL.
 */
AnimationLink* FindAnimationLink(AnimationLink::LinkList* pAnimList, AnimTransform* pAnimTrans);

} // namespace detail

} // namespace lyt
} // namespace nw4hbm

#endif
