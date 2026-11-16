// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_resourceAccessor.cpp
#include "nw4hbm/lyt/resourceAccessor.h"

namespace nw4hbm {
namespace lyt {

// The destructor is defined before the constructor: retail has it first.

/*
 * ResourceAccessor::~ResourceAccessor
 */
ResourceAccessor::~ResourceAccessor() {} // 0x803C8630

/*
 * ResourceAccessor::ResourceAccessor
 */
ResourceAccessor::ResourceAccessor() {} // 0x803C8670

/*
 * ResourceAccessor::GetFont
 *
 * The base accessor holds no fonts.
 */
ut::Font* ResourceAccessor::GetFont(const char* name) {
    return NULL;
} // 0x803C8680

} // namespace lyt
} // namespace nw4hbm
