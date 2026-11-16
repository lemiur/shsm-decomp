#ifndef NW4HBM_MATH_TYPES_H
#define NW4HBM_MATH_TYPES_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/math/types.h

#include <revolution/types.h>
#include <revolution/mtx.h>

namespace nw4hbm {
namespace math {

/*
 * _VEC2
 *
 * Plain two-component vector; VEC2 adds the constructors and conversions.
 */
struct _VEC2 {
    f32 x;
    f32 y;
};

/*
 * _VEC3
 *
 * Plain three-component vector; VEC3 adds the constructors and conversions.
 */
struct _VEC3 {
    f32 x;
    f32 y;
    f32 z;
};

/*
 * _MTX34
 *
 * 3x4 row-major matrix, laid out as the MTX library's Mtx.
 */
struct _MTX34 {
    union {
        struct {
            f32 _00, _01, _02, _03;
            f32 _10, _11, _12, _13;
            f32 _20, _21, _22, _23;
        };
        f32 m[3][4];
        f32 a[3 * 4];
        Mtx mtx;
    };
};

/*
 * VEC2
 */
struct VEC2 : public _VEC2 {
    VEC2() {}
    VEC2(f32 fx, f32 fy) {
        x = fx;
        y = fy;
    }

    operator f32*() { return reinterpret_cast<f32*>(this); }
    operator const f32*() const { return reinterpret_cast<const f32*>(this); }
};

/*
 * VEC3
 */
struct VEC3 : public _VEC3 {
    VEC3() {}
    VEC3(f32 fx, f32 fy, f32 fz) {
        x = fx;
        y = fy;
        z = fz;
    }

    operator Vec*() { return reinterpret_cast<Vec*>(this); }
    operator const Vec*() const { return reinterpret_cast<const Vec*>(this); }
};

/*
 * MTX34
 */
struct MTX34 : public _MTX34 {
    MTX34() {}

    operator MtxPtr() { return mtx; }
    operator CMtxPtr() const { return mtx; }
};

/*
 * MTX34Identity
 *
 * Sets *pOut to the identity matrix; returns pOut.
 */
inline MTX34* MTX34Identity(MTX34* pOut) {
    PSMTXIdentity(*pOut);
    return pOut;
}

/*
 * MTX34Mult
 *
 * Stores p1 * p2 in *pOut; returns pOut.
 */
inline MTX34* MTX34Mult(MTX34* pOut, const MTX34* p1, const MTX34* p2) {
    PSMTXConcat(*p1, *p2, *pOut);
    return pOut;
}

/*
 * MTX34Copy
 *
 * Copies *p to *pOut; returns pOut.
 */
inline MTX34* MTX34Copy(MTX34* pOut, const MTX34* p) {
    PSMTXCopy(*p, *pOut);
    return pOut;
}

} // namespace math
} // namespace nw4hbm

#endif
