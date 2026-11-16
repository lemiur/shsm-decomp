// Reference: doldecomp/ogws src/revolution/NdevExi2AD/DebuggerDriver.c
// Unit name from doldecomp/ogws (NdevExi2AD). Built with GC/3.0a5 -O0 -inline off (configure.py): unoptimised code
// gives registers to the most-used locals first, ties to the first declared, so DBWrite's declaration order sets them.
#include <revolution/hardware.h>
#include <revolution/os.h>
#include <revolution/NdevExi2AD/DebuggerDriver.h>
#include <revolution/NdevExi2AD/ODEMU.h>

static u32 __DBRecvDataSize;
static u32 __DBRecvMail;
static u8 __DBEXIInputFlag;
static DBIntrCallback __DBDbgCallback;
static DBMtrCallback __DBMtrCallback;

static u8 __DBReadUSB_CSR(void);
static void __DBWaitForSendMail(void);

static BOOL __DBReadMailbox(u32* mailOut);
static BOOL __DBRead(u32 ofs, void* dest, u32 size);
static BOOL __DBWriteMailbox(u32 mail);
static BOOL __DBWrite(u32 ofs, const void* src, u32 size);

/*
 * __DBMtrHandler
 *
 * Handles the debugger's EXI interrupt by setting the input-ready flag, then forwards to the registered monitor callback if one is set.
 */
void __DBMtrHandler(__OSInterrupt type, OSContext* ctx) {
    __DBEXIInputFlag = TRUE;

    if (__DBMtrCallback != NULL)
        __DBMtrCallback(0, ctx);
} // 0x801900E8

/*
 * __DBIntrHandler
 *
 * Acknowledges the PI debug interrupt by writing 0x1000 to the first PI register, then calls the registered debugger callback with the interrupt type and context.
 */
void __DBIntrHandler(__OSInterrupt type, OSContext* ctx) {
    __PIRegs[0] = 0x1000;
    if (__DBDbgCallback)
        __DBDbgCallback(type, ctx);
} // 0x80190134

/*
 * __DBCheckMailBox
 *
 * Reads the USB control/status register and, when bit 3 is clear, reads the mailbox. A valid mail is stored along with its data size and the input-ready flag is set.
 */
static void __DBCheckMailBox(void) {
    u8 csr;
    u32 mail;

    csr = __DBReadUSB_CSR();
    if (!(csr & 0x8)) {
        __DBReadMailbox(&mail);
        if (ODEMUIsValidMail(mail)) {
            __DBRecvMail = mail;
            __DBRecvDataSize = ODEMUGetSize(mail);
            __DBEXIInputFlag = TRUE;
        }
    }
} // 0x80190184

/*
 * __DBReadUSB_CSR
 *
 * Reads the one-byte USB control/status register of the debugger device over EXI2 and returns it.
 */
static u8 __DBReadUSB_CSR(void) {
    u8 val;
    __DBEXIReadReg(0x34000000, &val, sizeof(val));
    return val;
} // 0x801901F4

/*
 * DBInitComm
 *
 * With interrupts disabled, hands back a pointer to the input-ready flag, stores the monitor callback and initialises the EXI2 link.
 */
void DBInitComm(u8** flagOut, DBMtrCallback handler) {
    BOOL enabled = OSDisableInterrupts();

    *flagOut = &__DBEXIInputFlag;
    __DBMtrCallback = handler;
    __DBEXIInit();

    OSRestoreInterrupts(enabled);
} // 0x80190224

/*
 * DBInitInterrupts
 *
 * Masks the EXI2 and PI debug interrupts, installs __DBMtrHandler as the debug callback and __DBIntrHandler as the PI debug interrupt handler, then unmasks the PI debug interrupt.
 */
void DBInitInterrupts(void) {
    __OSMaskInterrupts(OS_INTERRUPTMASK_EXI_2);
    __OSMaskInterrupts(OS_INTERRUPTMASK_PI_DEBUG);
    __DBDbgCallback = __DBMtrHandler;
    __OSSetInterruptHandler(__OS_INTERRUPT_PI_DEBUG, __DBIntrHandler);
    __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_DEBUG);
} // 0x8019027C

/*
 * DBQueryData
 *
 * Clears the input-ready flag and, if no received data is pending, checks the mailbox with interrupts disabled. Returns the pending received data size.
 */
u32 DBQueryData(void) {
    BOOL enabled;

    __DBEXIInputFlag = FALSE;

    if (__DBRecvDataSize == 0) {
        enabled = OSDisableInterrupts();
        __DBCheckMailBox();
        OSRestoreInterrupts(enabled);
    }

    return __DBRecvDataSize;
} // 0x801902D4

/*
 * DBRead
 *
 * With interrupts disabled, reads the pending received data into the destination buffer (size rounded up to a multiple of 4) from the offset given by the received mail. Clears the pending size and flag, and returns FALSE.
 */
BOOL DBRead(void* dst, u32 size) {
    u32 ofs;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    ofs = ODEMUGetPc2NngcOffset(__DBRecvMail) + 0x1000;
    __DBRead(ofs, dst, (size + 3) & ~3);
    __DBRecvDataSize = 0;
    __DBEXIInputFlag = FALSE;

    OSRestoreInterrupts(enabled);

    return FALSE;
} // 0x80190324

/*
 * DBWrite
 *
 * With interrupts disabled, waits for the mailbox to be free, writes the data (size rounded up to a multiple of 4) into the alternating half of the transfer area, then sends a mail with the offset counter and size. Returns FALSE.
 */
BOOL DBWrite(const void* src, u32 size) {
    static u8 l_byOffsetCounter = 128;

    u32 ofs;
    u32 mail;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    __DBWaitForSendMail();

    l_byOffsetCounter++;
    ofs = (l_byOffsetCounter & 0x1) ? 0x800 : 0;

    while (__DBWrite(ofs, src, (size + 3) & ~3) == 0) {
    }

    __DBWaitForSendMail();

    mail = ODEMUGenMailData(l_byOffsetCounter, size);
    while (__DBWriteMailbox(mail) == 0) {
    }

    __DBWaitForSendMail();

    OSRestoreInterrupts(enabled);

    return FALSE;
} // 0x801903A0

/*
 * __DBWaitForSendMail
 *
 * Polls the USB control/status register until bit 2 is clear.
 */
static void __DBWaitForSendMail(void) {
    u8 csr;

    do {
        csr = __DBReadUSB_CSR();
    } while (csr & 0x4);
} // 0x8019045C

/*
 * DBOpen
 *
 * Empty; opening the debugger channel needs no work.
 */
void DBOpen(void) {} // 0x80190498

/*
 * DBClose
 *
 * Empty; closing the debugger channel needs no work.
 */
void DBClose(void) {} // 0x8019049C

/*
 * ODEMUGetSize
 *
 * Returns the data size held in the low 13 bits of a mail.
 */
static u32 ODEMUGetSize(u32 mail) {
    return mail & 0x1FFF;
} // 0x801904A0

/*
 * ODEMUIsValidMail
 *
 * Returns whether the top byte of a mail carries the 0x1F marker bits.
 */
static BOOL ODEMUIsValidMail(u32 mail) {
    return (mail & 0x1F000000) == 0x1F000000;
} // 0x801904A8

/*
 * ODEMUGetPc2NngcOffset
 *
 * Returns the transfer-area offset for a mail: 0x800 if its page number is odd, otherwise 0.
 */
static u32 ODEMUGetPc2NngcOffset(u32 mail) {
    return (ODEMUGetPage(mail) & 0x1) ? 0x800 : 0;
} // 0x801904C0

/*
 * ODEMUGetPage
 *
 * Returns the page number held in bits 16 to 23 of a mail.
 */
static u32 ODEMUGetPage(u32 mail) {
    return (mail & 0xFF0000) >> 16;
} // 0x8019050C

/*
 * ODEMUGenMailData
 *
 * Builds a mail word from an offset counter (bits 16 to 23), the 0x1F000000 marker and a size (low 13 bits).
 */
static u32 ODEMUGenMailData(u32 ofs, u32 size) {
    return (ofs << 16 & 0xFF0000) | 0x1f000000 | size & 0x1fff;
} // 0x80190518

/*
 * __DBReadMailbox
 *
 * Reads the 32-bit mailbox register of the debugger device over EXI2.
 */
static BOOL __DBReadMailbox(u32* mailOut) {
    return __DBEXIReadReg(0x34000200, mailOut, sizeof(*mailOut));
} // 0x8019052C

/*
 * __DBRead
 *
 * Converts an offset into a RAM read command and reads the given number of bytes from the debugger device over EXI2.
 */
static BOOL __DBRead(u32 ofs, void* dest, u32 size) {
    return __DBEXIReadRam(((ofs + 0xD10000) << 6) & 0x3FFFFF00, dest, size);
} // 0x80190560

/*
 * __DBWriteMailbox
 *
 * Writes a 32-bit mail to the mailbox register of the debugger device over EXI2.
 */
static BOOL __DBWriteMailbox(u32 mail) {
    return __DBEXIWriteReg(0xB4000100, &mail, sizeof(mail));
} // 0x801905A0

/*
 * __DBWrite
 *
 * Converts an offset into a RAM write command and writes the given number of bytes to the debugger device over EXI2.
 */
static BOOL __DBWrite(u32 ofs, const void* src, u32 size) {
    return __DBEXIWriteRam((((ofs + 0xD10000) << 6) & 0x3FFFFF00) | 0x80000000, src, size);
} // 0x801905D4
