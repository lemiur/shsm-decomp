#ifndef NW4HBM_LYT_WINDOW_H
#define NW4HBM_LYT_WINDOW_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/window.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_window.h

#include <revolution/types.h>
#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"

namespace nw4hbm {
namespace ut {
namespace detail {
class RuntimeTypeInfo;
} // namespace detail
} // namespace ut

namespace lyt {

class Material;
struct ResBlockSet;

/*
 * InflationLRTB
 *
 * How far the content of a window reaches past the inner edges of its frame, per side.
 */
struct InflationLRTB {
    f32 l; // 0x00
    f32 r; // 0x04
    f32 t; // 0x08
    f32 b; // 0x0C
}; // size 0x10

/*
 * WindowFrameSize
 *
 * Thickness of a window's frame, per side.
 */
struct WindowFrameSize {
    f32 l; // 0x00
    f32 r; // 0x04
    f32 t; // 0x08
    f32 b; // 0x0C
}; // size 0x10

namespace res {

static const u32 SIGNATURE_WINDOW_BLOCK = 'wnd1';

/*
 * res::Window
 *
 * Layout-file data block of a window pane ("wnd1"): the pane block, the content inflation and
 * the offsets (from the block) of the content and of the frame offset table.
 */
struct Window : public Pane {
    InflationLRTB inflation;    // 0x4C
    u8 frameNum;                // 0x5C
    u8 padding1;                // 0x5D // pad: SDK res::Window.padding1
    u8 padding2;                // 0x5E // pad: SDK res::Window.padding2
    u8 padding3;                // 0x5F // pad: SDK res::Window.padding3
    u32 contentOffset;          // 0x60
    u32 frameOffsetTableOffset; // 0x64
}; // size 0x68

/*
 * res::WindowFrame
 *
 * One frame part of a window: its material and how its texture is flipped.
 */
struct WindowFrame {
    u16 materialIdx; // 0x00
    u8 textureFlip;  // 0x02
    u8 padding1;     // 0x03 // pad: SDK res::WindowFrame.padding1
}; // size 0x04

/*
 * res::WindowContent
 *
 * The content of a window: corner colours and material; its texture coordinates follow.
 */
struct WindowContent {
    u32 vtxCols[4];  // 0x00
    u16 materialIdx; // 0x10
    u8 texCoordNum;  // 0x12
    u8 padding[1];   // 0x13 // pad: SDK res::WindowContent.padding
}; // size 0x14

} // namespace res

/*
 * Window
 *
 * A pane drawn as a content quad surrounded by a frame of one, four or eight textured parts.
 */
class Window : public Pane {
public:
    // Number of corners, and so of vertex colours.
    enum {
        VERTEXCOLOR_MAX = 4,
    };

private:
    /*
     * Window::Content
     *
     * The content quad: corner colours and texture coordinates (its material is the pane's).
     */
    struct Content {
        ut::Color vtxColors[VERTEXCOLOR_MAX]; // 0x00
        detail::TexCoordAry texCoordAry;      // 0x10
    }; // size 0x18

    /*
     * Window::Frame
     *
     * One frame part: how its texture is flipped, and its material.
     */
    struct Frame {
        u8 textureFlip;      // 0x00
        Material* pMaterial; // 0x04
    }; // size 0x08

public:
    Window(const res::Window* pBlock, const ResBlockSet& resBlockSet);
    virtual ~Window();

    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const { return &typeInfo; }

    virtual void DrawSelf(const DrawInfo& drawInfo);
    virtual void AnimateSelf(u32 option);
    virtual ut::Color GetVtxColor(u32 idx) const;
    virtual void SetVtxColor(u32 idx, ut::Color value);
    virtual u8 GetVtxColorElement(u32 idx) const;
    virtual void SetVtxColorElement(u32 idx, u8 value);
    virtual Material* FindMaterialByName(const char* findName, bool bRecursive);
    virtual void UnbindAnimationSelf(AnimTransform* pAnimTrans);
    virtual AnimationLink* FindAnimationLink(AnimTransform* pAnimTrans);
    virtual void SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable, bool bRecursive);

    virtual Material* GetContentMaterial() const;
    virtual Material* GetFrameMaterial(u32 frameIdx) const;
    virtual void DrawContent(const math::VEC2& basePt, const WindowFrameSize& frameSize, u8 alpha);
    virtual void DrawFrame(const math::VEC2& basePt, const Frame& frame, const WindowFrameSize& frameSize,
                           u8 alpha);
    virtual void DrawFrame4(const math::VEC2& basePt, const Frame* frames, const WindowFrameSize& frameSize,
                            u8 alpha);
    virtual void DrawFrame8(const math::VEC2& basePt, const Frame* frames, const WindowFrameSize& frameSize,
                            u8 alpha);

    WindowFrameSize GetFrameSize(u8 frameNum, const Frame* frames);

    static const ut::detail::RuntimeTypeInfo typeInfo;

private:
    InflationLRTB mContentInflation; // 0xD4
    Content mContent;                // 0xE4
    Frame* mFrames;                  // 0xFC
    u8 mFrameNum;                    // 0x100
}; // size 0x104

} // namespace lyt
} // namespace nw4hbm

#endif
