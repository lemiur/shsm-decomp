#ifndef CLIMAX_SHUTILS_H
#define CLIMAX_SHUTILS_H

#include <revolution/types.h>

namespace RWS {
namespace SHUtils {

/* The case-folded hash of str, read at most maxLength characters (-1: to its NUL). Defined at 0x801B605C. */
u32 GetStringHashForceLower( const char* str, int maxLength );

}  // namespace SHUtils
}  // namespace RWS

#endif
