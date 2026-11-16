// Reference: zeldaret/tp include/TRK_MINNOW_DOLPHIN/utils/gc/MWCriticalSection_gc.h

#ifndef TRK_MWCRITICALSECTION_GC_H
#define TRK_MWCRITICALSECTION_GC_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Critical section on the GameCube-style target: the section word holds the interrupt state saved on entry. */
void MWInitializeCriticalSection(u32* section);
/* Disables interrupts and saves the previous state in *section. */
void MWEnterCriticalSection(u32* section);
/* Restores the interrupt state saved in *section. */
void MWExitCriticalSection(u32* section);

#ifdef __cplusplus
}
#endif

#endif /* TRK_MWCRITICALSECTION_GC_H */
