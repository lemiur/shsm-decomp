// Reference: doldecomp/ogws src/revolution/NdevExi2AD/exi2.c
// Built with GC/3.0a5 -O0 -inline off (configure.py). The four `twui` stubs after this unit are MetroTRK targsupp
// (TRKAccessFile..TRKPositionFile), not exi2.
#include <revolution/exi.h>
#include <revolution/hardware.h>
#include <revolution/os.h>
#include <revolution/NdevExi2AD/DebuggerDriver.h>

/* Channel 2 register words: control/parameter, control, data. */
#define EXI2_CPR (__EXIRegs[10])
#define EXI2_CR (__EXIRegs[13])
#define EXI2_DATA (__EXIRegs[14])

#define EXI_CPR_CS0B 0x80
#define EXI_CPR_INTMASKS 0x405
#define EXI_CR_TSTART 1

static u32 __EXISwap32(u32 val);

/*
 * __EXI2Select
 *
 * Asserts chip select on EXI channel 2, preserving the interrupt mask bits and selecting the 16 MHz clock. Returns TRUE.
 */
static BOOL __EXI2Select(void) {
    u32 cpr = EXI2_CPR;

    cpr &= EXI_CPR_INTMASKS;
    cpr |= EXI_CPR_CS0B | (EXI_FREQ_16M << 4);
    EXI2_CPR = cpr;

    return TRUE;
} // 0x80190618

/*
 * __EXI2Deselect
 *
 * Deasserts chip select on EXI channel 2 by clearing every bit of its parameter register except the interrupt mask bits. Returns TRUE.
 */
static BOOL __EXI2Deselect(void) {
    u32 cpr = EXI2_CPR;

    EXI2_CPR = cpr & EXI_CPR_INTMASKS;

    return TRUE;
} // 0x80190648

/*
 * __EXI2Sync
 *
 * Busy-waits until the transfer-start bit of the EXI channel 2 control register clears. Returns TRUE.
 */
static BOOL __EXI2Sync(void) {
    while (EXI2_CR & EXI_CR_TSTART) {
        ;
    }

    return TRUE;
} // 0x80190674

/*
 * __EXI2Imm
 *
 * Performs an immediate EXI channel 2 transfer of up to four bytes. For a write the bytes are packed into the data register, for a read they are unpacked from it after the transfer completes. Returns TRUE.
 */
BOOL __EXI2Imm(void* mem, s32 size, u32 type) {
    u8* bmem;
    u32 imm;
    u32 data;
    int i;
    int j;

    if (type != EXI_READ) {
        imm = 0;

        for (i = 0; i < size; i++) {
            imm |= ((u8*)mem)[i] << (3 - i) * 8;
        }

        EXI2_DATA = imm;
    }

    EXI2_CR = type << 2 | EXI_CR_TSTART | (size - 1) * 16;

    __EXI2Sync();

    if (type == EXI_READ) {
        bmem = (u8*)mem;
        data = EXI2_DATA;

        for (j = 0; j < size; j++) {
            *bmem++ = data >> (3 - j) * 8;
        }
    }

    return TRUE;
} // 0x80190690

/*
 * __DBEXIInit
 *
 * Masks the EXI2 interrupt, waits for any transfer to finish, clears the channel 2 parameter register and writes 0xD4 to the debugger device's register at command 0xB4000000.
 */
void __DBEXIInit(void) {
    u8 val;

    __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_2);
    while ((EXI2_CR & EXI_CR_TSTART) == 1) {
    }
    EXI2_CPR = 0;

    val = 0xD4;
    __DBEXIWriteReg(0xB4000000, &val, sizeof(val));
} // 0x80190768

/*
 * __DBEXIReadReg
 *
 * Reads a 1, 2 or 4 byte register of the debugger device over EXI2: sends the command word, receives a 32-bit value and stores it byte-swapped to the buffer. Returns TRUE if no step failed.
 */
BOOL __DBEXIReadReg(u32 cmd, void* mem, s32 size) {
    BOOL error = FALSE;
    u32 read_val = 0;

    error = error | !__EXI2Select();
    error = error | !__EXI2Imm(&cmd, sizeof(cmd), EXI_WRITE);
    error = error | !__EXI2Sync();
    error = error | !__EXI2Imm(&read_val, sizeof(read_val), EXI_READ);
    error = error | !__EXI2Sync();
    error = error | !__EXI2Deselect();

    switch (size) {
    case 1:
        *(u8*)mem = read_val >> 24;
        break;
    case 2:
        *(u16*)mem = read_val >> 24 | read_val >> 8 & 0xFF00;
        break;
    default:
        *(u32*)mem = __EXISwap32(read_val);
        break;
    }

    return error == FALSE;
} // 0x801907C8

/*
 * __DBEXIWriteReg
 *
 * Writes a 1, 2 or 4 byte value to a register of the debugger device over EXI2: sends the command word, then the value packed into a 32-bit word. Returns TRUE if no step failed.
 */
BOOL __DBEXIWriteReg(u32 cmd, const void* mem, s32 size) {
    BOOL error = FALSE;
    u32 write_val;

    switch (size) {
    case 1:
        write_val = (*(u8*)mem & 0xFF) << 24;
        break;
    case 2:
        write_val = (*(u16*)mem & 0xFF) << 24 | (*(u16*)mem & 0xFF00) << 8;
        break;
    default:
        write_val = __EXISwap32(*(u32*)mem);
        break;
    }

    error = error | !__EXI2Select();
    error = error | !__EXI2Imm(&cmd, sizeof(cmd), EXI_WRITE);
    error = error | !__EXI2Sync();
    error = error | !__EXI2Imm(&write_val, sizeof(write_val), EXI_WRITE);
    error = error | !__EXI2Sync();
    error = error | !__EXI2Deselect();

    return error == FALSE;
} // 0x801908E8

/*
 * __DBEXIReadRam
 *
 * Reads RAM from the debugger device over EXI2: sends the command word, then receives the data one 32-bit word at a time into the buffer. Returns TRUE if no step failed.
 */
BOOL __DBEXIReadRam(u32 cmd, void* mem, s32 size) {
    BOOL error = FALSE;
    u32 read_val;
    u32* lmem = (u32*)mem;

    error = error | !__EXI2Select();
    error = error | !__EXI2Imm(&cmd, sizeof(cmd), EXI_WRITE);
    error = error | !__EXI2Sync();

    for (; size > 0; size -= 4) {
        error = error | !__EXI2Imm(&read_val, sizeof(read_val), EXI_READ);
        error = error | !__EXI2Sync();
        *lmem++ = read_val;
    }

    error = error | !__EXI2Deselect();

    return error == FALSE;
} // 0x80190A04

/*
 * __DBEXIWriteRam
 *
 * Writes RAM to the debugger device over EXI2: sends the command word, then sends the buffer one 32-bit word at a time. Returns TRUE if no step failed.
 */
BOOL __DBEXIWriteRam(u32 cmd, const void* mem, s32 size) {
    BOOL error = FALSE;
    u32 write_val;
    u32* lmem = (u32*)mem;

    error = error | !__EXI2Select();
    error = error | !__EXI2Imm(&cmd, sizeof(cmd), EXI_WRITE);
    error = error | !__EXI2Sync();

    for (; size > 0; size -= 4) {
        write_val = *lmem++;
        error = error | !__EXI2Imm(&write_val, sizeof(write_val), EXI_WRITE);
        error = error | !__EXI2Sync();
    }

    error = error | !__EXI2Deselect();

    return error == FALSE;
} // 0x80190AE8

/*
 * __EXISwap32
 *
 * Returns the 32-bit value with its byte order reversed.
 */
static u32 __EXISwap32(u32 val) {
    return val >> 24 & 0x000000FF | val >> 8 & 0x0000FF00 | val << 8 & 0x00FF0000 | val << 24 & 0xFF000000;
} // 0x80190BCC
