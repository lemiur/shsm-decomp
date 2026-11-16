// Reference: doldecomp/ogws src/revolution/AX/AXProf.c
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>

static BOOL __AXProfileInitialized = FALSE;
static AXPROFILE* __AXProfile;
static u32 __AXMaxProfiles;
static u32 __AXCurrentProfile;

/*
 * __AXGetCurrentProfile
 *
 * Hands out the next slot of the profile ring, or NULL while profiling is off.
 */
AXPROFILE* __AXGetCurrentProfile(void) {
    if (__AXProfileInitialized) {
        AXPROFILE* prof = &__AXProfile[__AXCurrentProfile];

        __AXCurrentProfile++;
        __AXCurrentProfile %= __AXMaxProfiles;

        return prof;
    }

    return NULL;
} // 0x80230990
