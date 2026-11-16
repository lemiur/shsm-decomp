// Reference: zeldaret/tp libs/revolution/src/exi/EXIUart.c
#include <revolution/os.h>
#include <revolution/exi.h>

static s32 Chan;
static u32 Dev;
static u32 BarnacleEnabled;
static u32 Enabled;

#define UART_ENABLED_MAGIC 0xA5FF005A

/*
 * ProbeBarnacle
 */
static BOOL ProbeBarnacle(s32 chan, u32 dev, u32* revision) {
    int err;
    u32 cmd;

    if (chan != 2 && dev == 0 && !EXIAttach(chan, NULL)) {
        return FALSE;
    }

    err = !EXILock(chan, dev, NULL);
    if (!err) {
        err = !EXISelect(chan, dev, EXI_FREQ_1M);
        if (!err) {
            cmd = 0x20011300;
            err = FALSE;
            err |= !EXIImm(chan, &cmd, sizeof(cmd), EXI_WRITE, NULL);
            err |= !EXISync(chan);
            err |= !EXIImm(chan, revision, sizeof(revision), EXI_READ, NULL);
            err |= !EXISync(chan);
            err |= !EXIDeselect(chan);
        }

        EXIUnlock(chan);
    }

    if (chan != 2 && dev == 0) {
        EXIDetach(chan);
    }

    if (err) {
        return FALSE;
    }

    return (*revision != 0xFFFFFFFF) ? TRUE : FALSE;
} // 0x8020F080

void __OSEnableBarnacle(s32 chan, u32 dev) {
    u32 id;

    if (!EXIGetID(chan, dev, &id)) {
        return;
    }

    switch (id) {
    case EXI_MEMORY_CARD_59:
    case EXI_MEMORY_CARD_123:
    case EXI_MEMORY_CARD_251:
    case EXI_MEMORY_CARD_507:
    case EXI_USB_ADAPTER:
    case EXI_NPDP_GDEV:
    case EXI_MODEM:
    case EXI_MARLIN:
    case 0x04020100:
    case EXI_ETHER:
    case 0x04020300:
    case 0x04220000:
    case EXI_RS232C:
    case EXI_MIC:
    case EXI_AD16:
    case EXI_STREAM_HANGER:
    case 0x80000004:
    case 0x80000008:
    case 0x80000010:
    case 0x80000020:
    case 0xFFFFFFFF:
        break;
    default:
        if (ProbeBarnacle(chan, dev, &id)) {
            Chan = chan;
            Dev = dev;
            Enabled = BarnacleEnabled = UART_ENABLED_MAGIC;
            break;
        }
    }
} // 0x8020F200
