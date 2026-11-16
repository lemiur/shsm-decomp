// Reference: zeldaret/tp libs/TRK_MINNOW_DOLPHIN/gamedev/cust_connection/utils/gc/MWCriticalSection_gc.c

#include <revolution/os.h>

#include "TRK/MWCriticalSection_gc.h"

/* Nothing to set up: the section is just the saved interrupt state. */
void MWInitializeCriticalSection(u32* section) {} // 0x80190E84

/* Disables interrupts and keeps the previous state in *section. */
void MWEnterCriticalSection(u32* section) {
    *section = OSDisableInterrupts();
} // 0x80190E88

/* Restores the interrupt state saved by MWEnterCriticalSection. */
void MWExitCriticalSection(u32* section) {
    OSRestoreInterrupts(*section);
} // 0x80190EB8
