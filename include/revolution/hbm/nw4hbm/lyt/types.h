#ifndef NW4HBM_LYT_TYPES_H
#define NW4HBM_LYT_TYPES_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/types.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_types.h

#include <revolution/types.h>

namespace nw4hbm {
namespace lyt {
namespace detail {

/*
 * ConvertOffsToPtr
 *
 * Layout files store the position of one block inside another as a byte offset from the start of
 * the enclosing block; this turns such an offset, taken from `pBase`, into a pointer to T.
 * The sum is formed on the address as an integer, as in the SDK: pointer arithmetic on a
 * `u8*` lets the optimiser merge the two offset sums of Material's constructor differently.
 */
template <typename T>
inline const T* ConvertOffsToPtr(const void* pBase, unsigned int offset) {
    return reinterpret_cast<const T*>(reinterpret_cast<unsigned int>(pBase) + offset);
}

/*
 * ConvertOffsToPtr
 *
 * As above, for data that may be written.
 */
template <typename T>
inline T* ConvertOffsToPtr(void* pBase, unsigned int offset) {
    return reinterpret_cast<T*>(reinterpret_cast<unsigned int>(pBase) + offset);
}

/*
 * SetBit
 *
 * Sets bit `pos` of *pBits to `val`.
 */
template <typename T>
inline void SetBit(T* pBits, int pos, bool val) {
    T mask(~(1 << pos));

    *pBits &= mask;
    *pBits |= (val ? 1 : 0) << pos;
}

/*
 * TestBit
 *
 * Returns whether bit `pos` of `bits` is set.
 */
template <typename T>
inline bool TestBit(T bits, int pos) {
    T mask(1 << pos);

    return (bits & mask) != 0;
}

/*
 * GetBits
 *
 * Extracts the `len` bits of `bits` that start at bit `pos`.
 */
template <typename T>
inline T GetBits(T bits, int pos, int len) {
    T mask(~(~0 << len));

    return bits >> pos & mask;
}

} // namespace detail
} // namespace lyt
} // namespace nw4hbm

#endif
