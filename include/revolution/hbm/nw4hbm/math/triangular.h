#ifndef NW4HBM_MATH_TRIANGULAR_H
#define NW4HBM_MATH_TRIANGULAR_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/math/triangular.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/math/math_triangular.h

#include <revolution/types.h>
#include "nw4hbm/math/constants.h"

namespace nw4hbm {
namespace math {

// Angles in "FIdx" units: 256 per full turn, as a float.

f32 SinFIdx(f32 fidx);
f32 CosFIdx(f32 fidx);
void SinCosFIdx(f32* s, f32* c, f32 fidx);
f32 AtanFIdx(f32 x);
f32 Atan2FIdx(f32 y, f32 x);

/*
 * SinDeg
 *
 * Sine of an angle in degrees.
 */
inline f32 SinDeg(f32 deg) {
    return SinFIdx(deg * convert::Deg2FIdx);
}

/*
 * CosDeg
 *
 * Cosine of an angle in degrees.
 */
inline f32 CosDeg(f32 deg) {
    return CosFIdx(deg * convert::Deg2FIdx);
}

/*
 * AtanDeg / AtanRad
 *
 * Arc tangent of a ratio, in degrees or radians.
 */
inline f32 AtanDeg(f32 x) {
    return AtanFIdx(x) * convert::FIdx2Deg;
}

inline f32 AtanRad(f32 x) {
    return AtanFIdx(x) * convert::FIdx2Rad;
}

/*
 * Atan2Deg / Atan2Rad
 *
 * Arc tangent of a vector (y, x), in degrees or radians.
 */
inline f32 Atan2Deg(f32 y, f32 x) {
    return Atan2FIdx(y, x) * convert::FIdx2Deg;
}

inline f32 Atan2Rad(f32 y, f32 x) {
    return Atan2FIdx(y, x) * convert::FIdx2Rad;
}

} // namespace math
} // namespace nw4hbm

#endif
