#ifndef MSL_ASSERT_H
#define MSL_ASSERT_H

// Reference: Metrowerks MSL assert.h
//
// The retail build is a release build (configure.py passes -DNDEBUG=1), so assert() compiles to
// nothing and __assert is not linked into the DOL. The library sources still state their
// preconditions with it, as the reference sources do.

#ifdef __cplusplus
extern "C" {
#endif

void __assert(const char* file, int line, const char* expression);

#ifdef __cplusplus
}
#endif

#ifdef NDEBUG
#define assert(expression) ((void)0)
#else
#define assert(expression) ((expression) ? (void)0 : __assert(__FILE__, __LINE__, #expression))
#endif

#endif
