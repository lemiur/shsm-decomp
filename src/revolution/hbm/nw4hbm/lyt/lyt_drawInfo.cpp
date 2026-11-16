// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_drawInfo.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_drawInfo.cpp
#include "nw4hbm/lyt/drawInfo.h"

#include <string.h>

namespace nw4hbm {
namespace lyt {

/*
 * DrawInfo::DrawInfo
 *
 * Identity view matrix, empty view rectangle, unit location scale, opaque
 * global alpha and every flag clear.
 */
DrawInfo::DrawInfo() : mLocationAdjustScale(1.0f, 1.0f), mGlobalAlpha(1.0f) {
    memset(&mFlag, 0, sizeof mFlag);
    math::MTX34Identity(&mViewMtx);
} // 0x803C2DC0

/*
 * DrawInfo::~DrawInfo
 */
DrawInfo::~DrawInfo() {} // 0x803C2E40

} // namespace lyt
} // namespace nw4hbm
