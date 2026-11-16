#ifndef REVOLUTION_NDEVEXI2AD_ODEMU_H
#define REVOLUTION_NDEVEXI2AD_ODEMU_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reference: doldecomp/ogws include/revolution/NdevExi2AD/ODEMU.h */

/* A mail word carries 0x1F in its top five bits, the buffer page in bits 16..23 and the byte count in bits 0..12. */
static u32 ODEMUGetSize(u32 mail);
static BOOL ODEMUIsValidMail(u32 mail);
static u32 ODEMUGetPc2NngcOffset(u32 mail);
static u32 ODEMUGetPage(u32 mail);
static u32 ODEMUGenMailData(u32 ofs, u32 size);

#ifdef __cplusplus
}
#endif

#endif
