#ifndef NW4HBM_UT_RECT_H
#define NW4HBM_UT_RECT_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/Rect.h

#include <revolution/types.h>
#include "nw4hbm/math/arithmetic.h"

namespace nw4hbm {
namespace ut {

/*
 * Rect
 *
 * Axis-aligned rectangle given by its edges; a default-constructed Rect is
 * all zero.
 */
struct Rect {
    Rect() : left(), top(), right(), bottom() {}
    Rect(f32 l, f32 t, f32 r, f32 b) : left(l), top(t), right(r), bottom(b) {}
    ~Rect() {}

    f32 GetWidth() const { return right - left; }
    f32 GetHeight() const { return bottom - top; }

    /*
     * MoveTo
     *
     * Moves the rectangle, keeping its size, so that its top-left corner is at (x, y).
     */
    void MoveTo(f32 x, f32 y) {
        right = x + GetWidth();
        left = x;

        bottom = y + GetHeight();
        top = y;
    }

    /*
     * Normalize
     *
     * Swaps the edges where needed so that left <= right and top <= bottom.
     */
    void Normalize() {
        f32 l = left;
        f32 t = top;
        f32 r = right;
        f32 b = bottom;

        left = math::FSelect(r - l, l, r);
        right = math::FSelect(r - l, r, l);
        top = math::FSelect(b - t, t, b);
        bottom = math::FSelect(b - t, b, t);
    }

    f32 left;   // 0x00
    f32 top;    // 0x04
    f32 right;  // 0x08
    f32 bottom; // 0x0C
};

} // namespace ut
} // namespace nw4hbm

#endif
