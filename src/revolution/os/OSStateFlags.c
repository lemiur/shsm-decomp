// Reference: zeldaret/tp libs/revolution/src/os/OSStateFlags.c
#include <revolution/os.h>
#include <revolution/nand.h>
#include <string.h>

static OSStateFlags StateFlags ATTRIBUTE_ALIGN(32);

/*
 * CheckSum
 *
 * Sums the words of flags that follow the checksum field.
 */
static u32 CheckSum(OSStateFlags* flags) {
    u32* ptr;
    int i;
    u32 sum;

    ptr = (u32*)&flags->lastBootApp;
    sum = 0;

    for (i = 0; i < (sizeof(OSStateFlags) - 4) / 4; i++) {
        sum += *ptr++;
    }

    return sum;
}

// Retail keeps the closing NANDClose test as a compare and branch; with the global optimizer on, the
// compiler folds the two explicit returns into one boolean value (cntlzw/srwi). The SDK source's own
// pragma is not recorded anywhere, so the pragma is inferred from the bytes, as in OS.c.
#pragma global_optimizer off

/*
 * __OSWriteStateFlags
 *
 * Writes flags, with its checksum filled in, to state.dat. Returns TRUE on success.
 */
BOOL __OSWriteStateFlags(OSStateFlags* flags) {
    NANDFileInfo fileInfo;
    s32 result;

    memcpy(&StateFlags, flags, sizeof(StateFlags));
    StateFlags.checkSum = CheckSum(&StateFlags);

    result = NANDOpen("/title/00000001/00000002/data/state.dat", &fileInfo, 2);

    if (result == 0) {
        result = NANDWrite(&fileInfo, &StateFlags, sizeof(StateFlags));
        if (result != sizeof(StateFlags)) {
            NANDClose(&fileInfo);
            return FALSE;
        }

        result = NANDClose(&fileInfo);
        if (result != 0) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    return TRUE;
} // 0x8021D4F0
#pragma global_optimizer reset

/*
 * __OSReadStateFlags
 *
 * Reads state.dat into flags. A missing, short or corrupt file yields zeroed flags
 * and FALSE; a short read also deletes the file.
 */
BOOL __OSReadStateFlags(OSStateFlags* flags) {
    NANDFileInfo fileInfo;
    s32 result;

    result = NANDOpen("/title/00000001/00000002/data/state.dat", &fileInfo, 1);
    if (result == 0) {
        result = NANDRead(&fileInfo, &StateFlags, sizeof(OSStateFlags));
        NANDClose(&fileInfo);

        if (result != sizeof(OSStateFlags)) {
            NANDDelete("/title/00000001/00000002/data/state.dat");
            memset(flags, 0, sizeof(StateFlags));
            return FALSE;
        }
    } else {
        memset(flags, 0, sizeof(OSStateFlags));
        return FALSE;
    }

    if (CheckSum(&StateFlags) != StateFlags.checkSum) {
        memset(flags, 0, sizeof(OSStateFlags));
        return FALSE;
    }

    memcpy(flags, &StateFlags, sizeof(OSStateFlags));
    return TRUE;
} // 0x8021D5E0
