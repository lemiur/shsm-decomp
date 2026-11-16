#ifndef __PPC_EABI_LINKER_H
#define __PPC_EABI_LINKER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exception table index range of one code fragment, emitted by the linker. */
typedef struct __eti_init_info {
    void* eti_start;
    void* eti_end;
    void* code_start;
    unsigned long code_size;
} __eti_init_info;

typedef void (*voidfunctionptr)(void);

/* Stack bounds, defined by the linker script (top of the stack and its lowest address). */
extern char _stack_addr[];
extern char _stack_end[];

/* Small-data base addresses (r13 and r2), defined by the linker. */
extern char _SDA_BASE_[];
extern char _SDA2_BASE_[];

/* One section __start copies from its load address to its run address. */
typedef struct __rom_copy_info {
    char* rom;          /* load address */
    char* addr;         /* run address */
    unsigned int size;  /* 0 ends the table */
} __rom_copy_info;

/* One section __start clears. */
typedef struct __bss_init_info {
    char* addr;
    unsigned int size;  /* 0 ends the table */
} __bss_init_info;

/* Bounds of the arena the OS hands out, defined by the linker script. */
extern char __ArenaLo[];
extern char __ArenaHi[];

__declspec(section ".init") extern __rom_copy_info _rom_copy_info[];
__declspec(section ".init") extern __bss_init_info _bss_init_info[];
__declspec(section ".init") extern __eti_init_info _eti_init_info[];

/* Null-terminated tables of static constructors and destructors. */
__declspec(section ".ctors") extern voidfunctionptr _ctors[];
__declspec(section ".dtors") extern voidfunctionptr _dtors[];

#ifdef __cplusplus
}
#endif

#endif
