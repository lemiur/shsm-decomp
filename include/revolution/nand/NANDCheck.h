#ifndef REVOLUTION_NAND_NANDCHECK_H
#define REVOLUTION_NAND_NANDCHECK_H

#include <revolution/nand.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-internal declarations owned by revolution/nand/NANDCheck.c. */

s32 nandCalcUsage(u32* fsBlock, u32* inode, const char* dir_list[]);

#ifdef __cplusplus
}
#endif

#endif
