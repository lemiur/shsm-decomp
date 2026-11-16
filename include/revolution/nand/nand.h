#ifndef REVOLUTION_NAND_NAND_H
#define REVOLUTION_NAND_NAND_H

#include <revolution/nand.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-internal declarations owned by revolution/nand/nand.c. */

void nandComposePerm(u8* perm, const u32 ownerAcc, const u32 groupAcc, const u32 othersAcc);

#ifdef __cplusplus
}
#endif

#endif
