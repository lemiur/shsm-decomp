#ifndef _MSL_LIMITS_H
#define _MSL_LIMITS_H

// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/include/stl/limits.h
// (CHAR_MIN/CHAR_MAX follow this project's signed plain char)

#define CHAR_BIT 8

#define MB_LEN_MAX 8

#define SCHAR_MIN (-0x7F - 1)
#define SCHAR_MAX 0x7F
#define UCHAR_MAX 0xFF

#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

#define SHRT_MIN (-0x7FFF - 1)
#define SHRT_MAX 0x7FFF
#define USHRT_MAX 0xFFFF

#define INT_MIN (-0x7FFFFFFF - 1)
#define INT_MAX 0x7FFFFFFF
#define UINT_MAX 0xFFFFFFFF

#define LONG_MIN (-0x7FFFFFFFL - 1)
#define LONG_MAX 0x7FFFFFFFL
#define ULONG_MAX 0xFFFFFFFFUL

#define LLONG_MIN (-0x7FFFFFFFFFFFFFFFLL - 1)
#define LLONG_MAX 0x7FFFFFFFFFFFFFFFLL
#define ULLONG_MAX 0xFFFFFFFFFFFFFFFFULL

#endif
