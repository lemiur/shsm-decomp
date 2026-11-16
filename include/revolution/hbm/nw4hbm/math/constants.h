#ifndef NW4HBM_MATH_CONSTANTS_H
#define NW4HBM_MATH_CONSTANTS_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/math/constants.h

#include <revolution/types.h>

namespace nw4hbm {
namespace math {

static const f32 Ln2 = 0.69314718056f;
static const f32 Pi = 3.141592653589f;
static const f32 Tau = 2.0f * Pi;

namespace convert {

static const f32 Deg2Rad = Pi / 180.0f;
static const f32 Deg2FIdx = 256.0f / 360.0f;
static const f32 FIdx2Deg = 360.0f / 256.0f;
static const f32 Rad2FIdx = 256.0f / Tau;
static const f32 FIdx2Rad = Tau / 256.0f;

} // namespace convert

} // namespace math
} // namespace nw4hbm

#endif
