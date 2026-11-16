#ifndef _MSL_ERRNO_H
#define _MSL_ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

#define EDOM 33
#define ERANGE 34
#define EFPOS 40

extern int errno;

#ifdef __cplusplus
}
#endif

#endif
