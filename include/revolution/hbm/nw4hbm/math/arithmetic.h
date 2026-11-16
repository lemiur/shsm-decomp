#ifndef NW4HBM_MATH_ARITHMETIC_H
#define NW4HBM_MATH_ARITHMETIC_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/math/arithmetic.h

#include <revolution/types.h>
#include <revolution/os/OSFastCast.h>

namespace nw4hbm {
namespace math {

/*
 * FAbs
 *
 * |x|, one fabs.
 */
inline f32 FAbs(f32 x) {
    return __fabsf(x);
}

/*
 * FSelect
 *
 * `ifPos` when `cond` >= 0, else `ifNeg`: one fsel, no branch.
 */
// ASM-HELPER: a single-precision fsel (the compiler's __fsel intrinsic is double and adds an frsp per
// result); the nw4hbm arithmetic.h form (ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/math/arithmetic.h)
inline f32 FSelect(register f32 cond, register f32 ifPos, register f32 ifNeg) {
    register f32 ret;

    asm { fsel ret, cond, ifPos, ifNeg }

    return ret;
}

/*
 * F32ToU16
 *
 * Converts through the fast-cast quantisation register (OSInitFastCast).
 */
inline u16 F32ToU16(f32 x) {
    u16 rval;

    OSf32tou16(&x, &rval);

    return rval;
}

/*
 * U16ToF32
 *
 * Converts through the fast-cast quantisation register (OSInitFastCast).
 */
inline f32 U16ToF32(u16 x) {
    f32 rval;

    OSu16tof32(&x, &rval);

    return rval;
}

} // namespace math
} // namespace nw4hbm

#endif
