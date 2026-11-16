// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/os/__start.c
// Reference: zeldaret/tp libs/revolution/src/os/__start.c
//
// The program entry point. __start runs from .init before anything is set up: it loads the registers and the
// stack, initialises the hardware, copies and clears the data sections, starts the debugger the disc's boot
// information asks for, then calls main with the arguments the BI2 carries.
#include <revolution/os.h>
#include <revolution/os/__ppc_eabi_init.h>
#include <revolution/db.h>
#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>
#include <TRK/dolphin_trk.h>
#include <stdlib.h>
#include <string.h>

/* Low-memory words __start reads and writes (cached addresses). */
#define OS_DB_EXCEPTION_MASK 0x80000044 /* DBInterface.exceptionMask of the block at 0x40 */
#define OS_ARENA_HI 0x80000034          /* OSBootInfo.arenaHi */
#define OS_DVD_BI2 0x800000F4           /* the disc's BI2 (boot information 2), or NULL */
#define OS_BI2_DEBUG_FLAG 0x800030E8    /* debug level when the BI2 is not loaded */
#define OS_DVD_DEVICE_CODE 0x800030E6   /* drive device code; bit 15 set once the inquiry has run */
#define OS_USABLE_MEM1_END 0x80003110

/* Fields of the BI2 that __start reads. */
#define BI2_ARGUMENT_OFS 0x8 /* offset from the BI2 to { argc, argv offsets[argc] }, or 0 */
#define BI2_DEBUG_FLAG 0xC   /* 2: TRK on GDEV, 3: TRK on NDEV, 4: TRK on the broadband adapter */

/* The game's entry point. */
int main(int argc, char** argv);

/* Set when the BI2 asks for the debugger on the broadband adapter; read after OSInit. */
static u8 Debug_BBA;

__declspec(section ".init") static void __init_registers(void);
__declspec(section ".init") static void __init_data(void);

/*
 * __check_pad3
 *
 * Restarts the console when the buttons held on GameCube pad 3 at boot include the 0x0EEF combination.
 */
__declspec(section ".init") static void __check_pad3(void) {
    if ((__OSGCPad3Button & 0x0EEF) == 0x0EEF) {
        OSResetSystem(FALSE, 0, FALSE);
    }
} // 0x800062C0

/*
 * __set_debug_bba
 */
__declspec(section ".init") static void __set_debug_bba(void) {
    Debug_BBA = TRUE;
} // 0x800062F0

/*
 * __get_debug_bba
 */
__declspec(section ".init") static BOOL __get_debug_bba(void) {
    return Debug_BBA;
} // 0x80006300

/*
 * __start
 *
 * Entry point: no stack and no small-data registers exist until __init_registers has run, and the argument
 * vector is unpacked in place in r14/r15, so the body is assembly.
 */
// ORIGINAL-ASM: an asm function in the SDK __start.c (xbret/xenoblade and zeldaret/tp have the same body)
__declspec(section ".init") __declspec(weak) asm void __start(void) {
    nofralloc

    bl __init_registers
    bl __init_hardware

    // First stack frame: back chain and saved LR are -1
    li r0, -1
    stwu r1, -8(r1)
    stw r0, 4(r1)
    stw r0, 0(r1)

    bl __init_data

    // Route no exception to the debugger yet
    li r0, 0
    lis r6, OS_DB_EXCEPTION_MASK@ha
    addi r6, r6, OS_DB_EXCEPTION_MASK@l
    stw r0, 0(r6)

    // Debug level: the loaded BI2's, else the low-memory copy when an arena is set
    lis r6, OS_DVD_BI2@ha
    addi r6, r6, OS_DVD_BI2@l
    lwz r6, 0(r6)
    cmplwi r6, 0
    beq _no_dvd_bi2
    lwz r7, BI2_DEBUG_FLAG(r6)
    b _handle_bi2_debug_flag

_no_dvd_bi2:
    lis r5, OS_ARENA_HI@ha
    addi r5, r5, OS_ARENA_HI@l
    lwz r5, 0(r5)
    cmplwi r5, 0
    beq _check_for_exec_args
    lis r7, OS_BI2_DEBUG_FLAG@ha
    addi r7, r7, OS_BI2_DEBUG_FLAG@l
    lwz r7, 0(r7)

    // Levels 2 and 3 start TRK now (r5 = 0 GDEV, 1 NDEV); level 4 defers to InitMetroTRK_BBA after OSInit
_handle_bi2_debug_flag:
    li r5, 0
    cmplwi r7, 2
    beq _call_init_metro_trk
    cmplwi r7, 3
    li r5, 1
    beq _call_init_metro_trk
    cmplwi r7, 4
    bne _check_for_exec_args
    li r5, 2
    bl __set_debug_bba
    b _check_for_exec_args

_call_init_metro_trk:
    lis r6, InitMetroTRK@ha
    addi r6, r6, InitMetroTRK@l
    mtlr r6
    blrl

    // Arguments: turn the BI2's argv offsets into pointers in place
_check_for_exec_args:
    lis r6, OS_DVD_BI2@ha
    addi r6, r6, OS_DVD_BI2@l
    lwz r5, 0(r6)
    cmplwi r5, 0
    beq+ _no_args
    lwz r6, BI2_ARGUMENT_OFS(r5)
    cmplwi r6, 0
    beq+ _no_args
    add r6, r5, r6
    lwz r14, 0(r6)
    cmplwi r14, 0
    beq _no_args
    addi r15, r6, 4
    mtctr r14

_unpack_args_loop:
    addi r6, r6, 4
    lwz r7, 0(r6)
    add r7, r7, r5
    stw r7, 0(r6)
    bdnz _unpack_args_loop

    // Keep the arguments out of the arena: lower arena hi and the MEM1 end below them
    lis r5, OS_ARENA_HI@ha
    addi r5, r5, OS_ARENA_HI@l
    clrrwi r7, r15, 5
    stw r7, 0(r5)
    lis r5, OS_USABLE_MEM1_END@ha
    addi r5, r5, OS_USABLE_MEM1_END@l
    clrrwi r7, r15, 5
    stw r7, 0(r5)
    b _init_os

_no_args:
    li r14, 0
    li r15, 0

_init_os:
    bl DBInit
    bl OSInit

    // Check pad 3 when the drive inquiry did not run or found device code 1
    lis r4, OS_DVD_DEVICE_CODE@ha
    addi r4, r4, OS_DVD_DEVICE_CODE@l
    lhz r3, 0(r4)
    andi. r5, r3, 0x8000
    beq _call_check_pad3
    andi. r3, r3, 0x7FFF
    cmplwi r3, 1
    bne _check_debug_bba
_call_check_pad3:
    bl __check_pad3

_check_debug_bba:
    bl __get_debug_bba
    cmplwi r3, 1
    bne _after_init_metro_trk_bba
    bl InitMetroTRK_BBA

_after_init_metro_trk_bba:
    bl __init_user
    mr r3, r14
    mr r4, r15
    bl main
    b exit
} // 0x80006310

/*
 * __copy_rom_section
 *
 * Copies one section from its load address to its run address and flushes it from the caches.
 */
__declspec(section ".init") static void __copy_rom_section(void* dst, const void* src, unsigned int size) {
    if (size == 0 || dst == src) {
        return;
    }

    memcpy(dst, src, size);
    __flush_cache(dst, size);
} // inlined (no copy in retail)

/*
 * __init_bss_section
 */
__declspec(section ".init") static void __init_bss_section(void* dst, unsigned int size) {
    if (size == 0) {
        return;
    }

    memset(dst, 0, size);
} // inlined (no copy in retail)

/*
 * __init_registers
 *
 * Clears the GPRs and loads the stack pointer and the two small-data bases from the linker's symbols.
 */
// ORIGINAL-ASM: an asm function in the SDK __start.c (xbret/xenoblade and zeldaret/tp have the same body)
__declspec(section ".init") static asm void __init_registers(void) {
    nofralloc
    li r0, 0
    li r3, 0
    li r4, 0
    li r5, 0
    li r6, 0
    li r7, 0
    li r8, 0
    li r9, 0
    li r10, 0
    li r11, 0
    li r12, 0
    li r14, 0
    li r15, 0
    li r16, 0
    li r17, 0
    li r18, 0
    li r19, 0
    li r20, 0
    li r21, 0
    li r22, 0
    li r23, 0
    li r24, 0
    li r25, 0
    li r26, 0
    li r27, 0
    li r28, 0
    li r29, 0
    li r30, 0
    li r31, 0
    lis r1, _stack_addr@h
    ori r1, r1, _stack_addr@l
    lis r2, _SDA2_BASE_@h
    ori r2, r2, _SDA2_BASE_@l
    lis r13, _SDA_BASE_@h
    ori r13, r13, _SDA_BASE_@l
    blr
} // 0x80006480

/*
 * __init_data
 *
 * Copies every section the linker lists in _rom_copy_info to its run address and clears every section in
 * _bss_init_info.
 */
__declspec(section ".init") static void __init_data(void) {
    __rom_copy_info* rom;
    __bss_init_info* bss;

    rom = _rom_copy_info;
    while (TRUE) {
        if (rom->size == 0) {
            break;
        }

        __copy_rom_section(rom->addr, rom->rom, rom->size);
        rom++;
    }

    bss = _bss_init_info;
    while (TRUE) {
        if (bss->size == 0) {
            break;
        }

        __init_bss_section(bss->addr, bss->size);
        bss++;
    }
} // 0x80006510
