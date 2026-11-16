#ifndef NW4HBM_UT_INLINES_H
#define NW4HBM_UT_INLINES_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/inlines.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {

/*
 * Min
 *
 * The smaller of two values.
 */
template <typename T>
inline T Min(T a, T b) {
    return a > b ? b : a;
}

/*
 * Max
 *
 * The larger of two values.
 */
template <typename T>
inline T Max(T a, T b) {
    return a < b ? b : a;
}

namespace {

/*
 * NonCopyable
 *
 * Private base of the classes that must not be copied: the copy constructor and
 * assignment are declared and never defined.
 */
class NonCopyable {
protected:
    NonCopyable() {}
    ~NonCopyable() {}

private:
    NonCopyable(const NonCopyable&);
    const NonCopyable& operator=(const NonCopyable&);
};

} // namespace
} // namespace ut
} // namespace nw4hbm

#endif // NW4HBM_UT_INLINES_H
