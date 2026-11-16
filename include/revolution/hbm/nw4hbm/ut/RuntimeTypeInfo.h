#ifndef NW4HBM_UT_RUNTIME_TYPE_INFO_H
#define NW4HBM_UT_RUNTIME_TYPE_INFO_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/RuntimeTypeInfo.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/ut/ut_RuntimeTypeInfo.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {
namespace detail {

/*
 * RuntimeTypeInfo
 *
 * Per-class RTTI record of the layout classes: a link to the base class's record.
 */
class RuntimeTypeInfo {
public:
    RuntimeTypeInfo(const RuntimeTypeInfo* parent) : mParentTypeInfo(parent) {}

    bool IsDerivedFrom(const RuntimeTypeInfo* typeInfo) const {
        const RuntimeTypeInfo* self;

        for (self = this; self; self = self->mParentTypeInfo) {
            if (self == typeInfo) {
                return true;
            }
        }

        return false;
    }

private:
    const RuntimeTypeInfo* mParentTypeInfo; // 0x00
}; // size 0x04

/*
 * GetTypeInfoFromPtr_
 *
 * The RTTI record of the class `T` points to.
 */
template <class T>
inline const RuntimeTypeInfo* GetTypeInfoFromPtr_(T*) {
    return &T::typeInfo;
}

} // namespace detail

/*
 * DynamicCast
 *
 * Casts `obj` to the pointer type `U` when its class derives from U's class,
 * else returns NULL (the layout classes' stand-in for dynamic_cast).
 */
template <typename U, class T>
inline U DynamicCast(T* obj) {
    const detail::RuntimeTypeInfo* typeInfoU = detail::GetTypeInfoFromPtr_(static_cast<U>(NULL));

    if (obj->GetRuntimeTypeInfo()->IsDerivedFrom(typeInfoU)) {
        return reinterpret_cast<U>(obj);
    }

    return NULL;
}
} // namespace ut
} // namespace nw4hbm

#endif
