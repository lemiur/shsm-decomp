#include <revolution/os.h>

/* Bounds of the arena the reboot code runs in. */
#define REBOOT_ARENA_LO ((void*)0x81280000)
#define REBOOT_ARENA_HI ((void*)0x812F0000)

/* Low-memory word (the disc partition type the previous program ran from) copied into __OSNextPartitionType before rebooting. */

static void* SaveStart = NULL;
static void* SaveEnd = NULL;

/*
 * __OSReboot
 *
 * Boots the DOL at bootDol on the disc with no arguments, passing
 * resetCode as the restart code.
 */
void __OSReboot(u32 resetCode, u32 bootDol) {
    const char* argv;

    OSDisableInterrupts();

    OSSetArenaLo(REBOOT_ARENA_LO);
    OSSetArenaHi(REBOOT_ARENA_HI);

    argv = NULL; /* an empty, NULL-terminated argument list */
    __OSNextPartitionType = __OSLaunchPartitionType;
    __OSBootDol(bootDol, resetCode | OS_RESET_RESTART, &argv);
} // 0x80218B70

/*
 * __OSGetSaveRegion
 *
 * Returns the bounds of the memory region preserved across a reboot.
 */
void __OSGetSaveRegion(void** start, void** end) {
    *start = SaveStart;
    *end = SaveEnd;
} // 0x80218BE0
