// Reference: zeldaret/tp libs/revolution/src/os/OSNandbootInfo.c
#include <revolution/os.h>
#include <revolution/nand.h>

/*
 * CheckSum
 *
 * Sums the words of info that follow the checksum field.
 */
static u32 CheckSum(OSNandbootInfo* info) {
    u32* ptr;
    u32 i;
    u32 sum;

    ptr = (u32*)&info->ArgOffset;
    sum = 0;
    for (i = 0; i < (sizeof(OSNandbootInfo) - 4) / 4; i++) {
        sum = sum + *ptr;
        ptr++;
    }

    return sum;
}

/*
 * __OSCreateNandbootInfo
 *
 * Makes sure /shared2/sys/NANDBOOTINFO exists with read/write permission for
 * everyone, recreating it if it has other permissions. Returns TRUE on success.
 */
BOOL __OSCreateNandbootInfo(void) {
    NANDStatus status;
    s32 result;

    result = NANDPrivateGetStatus("/shared2/sys/NANDBOOTINFO", &status);
    if (result == NAND_RESULT_OK && status.permission == NAND_PERM_RWALL) {
        return TRUE;
    } else if (result == NAND_RESULT_OK && status.permission != NAND_PERM_RWALL) {
        result = NANDPrivateDelete("/shared2/sys/NANDBOOTINFO");
        if (result != NAND_RESULT_OK) {
            return FALSE;
        }
    } else if (result != NAND_RESULT_NOEXISTS) {
        return FALSE;
    }

    result = NANDPrivateCreate("/shared2/sys/NANDBOOTINFO", NAND_PERM_RWALL, 0);
    if (result != NAND_RESULT_OK) {
        return FALSE;
    }

    return TRUE;
} // 0x8021DCB0

/*
 * __OSWriteNandbootInfo
 *
 * Fills in the checksum of info and writes it to /shared2/sys/NANDBOOTINFO.
 * info must be 32-byte aligned. Returns TRUE on success.
 */
BOOL __OSWriteNandbootInfo(OSNandbootInfo* info) {
    NANDFileInfo fileInfo;
    s32 result;

    info->CheckSum = CheckSum(info);
    result = NANDPrivateOpen("/shared2/sys/NANDBOOTINFO", &fileInfo, 2);

    if (result == NAND_RESULT_OK) {
        result = NANDWrite(&fileInfo, info, sizeof(OSNandbootInfo));
        if (result != sizeof(OSNandbootInfo)) {
            NANDClose(&fileInfo);
            return FALSE;
        }

        result = NANDClose(&fileInfo);
        if (result != NAND_RESULT_OK) {
            return FALSE;
        }
    } else {
        return FALSE;
    }

    return TRUE;
} // 0x8021DD60
