#ifndef NW4HBM_LYT_DRAW_INFO_H
#define NW4HBM_LYT_DRAW_INFO_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/drawInfo.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_drawInfo.h

#include <revolution/types.h>
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Rect.h"

namespace nw4hbm {
namespace lyt {

/*
 * DrawInfo
 *
 * The state a layout is drawn with: view matrix and rectangle, the location
 * adjustment scale, the global alpha and the drawing mode flags.
 */
class DrawInfo {
public:
    DrawInfo();
    virtual ~DrawInfo();

    const math::MTX34& GetViewMtx() const { return mViewMtx; }
    void SetViewMtx(const math::MTX34& value) { mViewMtx = value; }

    void SetViewRect(const ut::Rect& value) { mViewRect = value; }
    bool IsYAxisUp() const { return mViewRect.bottom - mViewRect.top < 0.0f; }

    const math::VEC2& GetLocationAdjustScale() const { return mLocationAdjustScale; }
    void SetLocationAdjustScale(const math::VEC2& scale) { mLocationAdjustScale = scale; }

    f32 GetGlobalAlpha() const { return mGlobalAlpha; }
    void SetGlobalAlpha(f32 alpha) { mGlobalAlpha = alpha; }

    bool IsMultipleViewMtxOnDraw() const { return mFlag.mulViewDraw; }
    bool IsInfluencedAlpha() const { return mFlag.influencedAlpha; }
    void SetInfluencedAlpha(bool bEnable) { mFlag.influencedAlpha = bEnable; }
    bool IsLocationAdjust() const { return mFlag.locationAdjust; }
    void SetLocationAdjust(bool bEnable) { mFlag.locationAdjust = bEnable; }
    bool IsInvisiblePaneCalculateMtx() const { return mFlag.invisiblePaneCalculateMtx; }
    bool IsDebugDrawMode() const { return mFlag.debugDrawMode; }

public:
    math::MTX34 mViewMtx;            // 0x04
    ut::Rect mViewRect;              // 0x34
    math::VEC2 mLocationAdjustScale; // 0x44
    f32 mGlobalAlpha;                // 0x4C, multiplied into every pane's alpha

    struct {
        u8 mulViewDraw : 1;
        u8 influencedAlpha : 1;
        u8 locationAdjust : 1;
        u8 invisiblePaneCalculateMtx : 1;
        u8 debugDrawMode : 1;
    } mFlag; // 0x50
};

} // namespace lyt
} // namespace nw4hbm

#endif
