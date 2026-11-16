// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/os/__ppc_eabi_init.h
#ifndef REVOLUTION_OS___PPC_EABI_INIT_H
#define REVOLUTION_OS___PPC_EABI_INIT_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Start-up steps __start calls, defined in __ppc_eabi_init.c. */
__declspec(section ".init") void __init_hardware(void);
__declspec(section ".init") void __flush_cache(void* address, unsigned int size);
void __init_user(void);

#ifdef __cplusplus
}
#endif

#endif
