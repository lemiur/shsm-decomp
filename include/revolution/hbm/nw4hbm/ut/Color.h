#ifndef NW4HBM_UT_COLOR_H
#define NW4HBM_UT_COLOR_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/Color.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_Color.h

#include <revolution/types.h>
#include <revolution/gx/GXStruct.h>

namespace nw4hbm {
namespace ut {

/*
 * Color
 *
 * GXColor with conversions to and from the packed 0xRRGGBBAA word. A
 * default-constructed Color is opaque white.
 *
 * Color is 4-byte aligned, as in the SDK (`alignas(4)` after the closing
 * brace; MWCC honours the attribute only in that position). Picture's
 * mVtxColors therefore starts at 0xD4, after Pane's one-byte member ends at
 * 0xD1.
 */
struct Color : public GXColor {
    Color() { *this = WHITE; }
    Color(u32 color) { *this = color; }
    Color(const GXColor& color) { *this = color; }

    ~Color() {}

    static const u32 WHITE = 0xFFFFFFFF;

    Color& operator=(u32 color) {
        ToU32ref() = color;
        return *this;
    }

    Color& operator=(const GXColor& color) { return *this = *reinterpret_cast<const u32*>(&color); }

    operator u32() const { return ToU32ref(); }

    u32& ToU32ref() { return *reinterpret_cast<u32*>(this); }
    const u32& ToU32ref() const { return *reinterpret_cast<const u32*>(this); }
} ATTRIBUTE_ALIGN(4); // size 0x04

} // namespace ut
} // namespace nw4hbm

#endif
