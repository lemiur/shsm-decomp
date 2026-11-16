#include <revolution/os.h>

/*
 * __OSModuleInit
 *
 * Empties the list of loaded modules at boot.
 */
void __OSModuleInit(void) {
    __OSModuleInfoList.head = __OSModuleInfoList.tail = NULL;
    __OSStringTable = NULL;
} // 0x80217CB0
