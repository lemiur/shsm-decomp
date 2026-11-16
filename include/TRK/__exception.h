// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/include/PowerPC_EABI_Support/MetroTRK/__exception.h

#ifndef TRK___EXCEPTION_H
#define TRK___EXCEPTION_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The nub's exception handlers, one 0x100-byte slot per PowerPC vector, at 0x80004000 in .init
 * (__exception.s). __TRK_copy_vectors copies the slots to the vector addresses. */
extern u8 gTRKInterruptVectorTable[];

#ifdef __cplusplus
}
#endif

#endif /* TRK___EXCEPTION_H */
