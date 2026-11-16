// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_bounding.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_bounding.cpp
#include "nw4hbm/lyt/bounding.h"

#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/drawInfo.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

namespace nw4hbm {
namespace lyt {

const ut::detail::RuntimeTypeInfo Bounding::typeInfo(&Pane::typeInfo);

/*
 * Bounding::Bounding
 */
Bounding::Bounding(const res::Bounding* pBlock, const ResBlockSet&) : Pane(pBlock) {} // 0x803C1DB0

/*
 * Bounding::~Bounding
 */
Bounding::~Bounding() {} // 0x803C1DF0

/*
 * Bounding::DrawSelf
 *
 * In debug draw mode, outlines the pane's rectangle in green.
 */
void Bounding::DrawSelf(const DrawInfo& drawInfo) {
    if (drawInfo.IsDebugDrawMode()) {
        LoadMtx(drawInfo);
        detail::DrawLine(GetVtxPos(), mSize, 0x00FF00FF); // green (RGBA)
    }
} // 0x803C1E50

} // namespace lyt
} // namespace nw4hbm
