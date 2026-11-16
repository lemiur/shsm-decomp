#ifndef REVOLUTION_OS_OSFASTCAST_H
#define REVOLUTION_OS_OSFASTCAST_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Paired-single quantisation types loaded into GQR2..GQR5 by OSInitFastCast (names from doldecomp/ogws). */
#define OS_GQR_TYPE_U8  4
#define OS_GQR_TYPE_U16 5
#define OS_GQR_TYPE_S8  6
#define OS_GQR_TYPE_S16 7

// ASM-HELPER: OSInitFastCast sets the paired-single quantisation registers GQR2..GQR5 (mtspr has no C spelling);
// an inline asm function in the RVL SDK OSFastCast.h (doldecomp/ogws include/revolution/OS/OSFastCast.h)
static inline void OSInitFastCast(void) {
    asm {
        li r3, 4
        oris r3, r3, 4
        mtspr GQR2, r3
        li r3, 5
        oris r3, r3, 5
        mtspr GQR3, r3
        li r3, 6
        oris r3, r3, 6
        mtspr GQR4, r3
        li r3, 7
        oris r3, r3, 7
        mtspr GQR5, r3
    }
}

/* Paired-single quantisation registers OSInitFastCast loads, one per type. */
#define OS_FASTCAST_U8  2
#define OS_FASTCAST_U16 3
#define OS_FASTCAST_S8  4
#define OS_FASTCAST_S16 5

// ASM-HELPER: a float load through the u16 quantisation register (psq_l has no C spelling); the RVL SDK
// OSFastCast.h form (xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h)
static inline f32 __OSu16tof32(register const u16* arg) {
    register f32 ret;

    asm { psq_l ret, 0(arg), 1, OS_FASTCAST_U16 }

    return ret;
}

// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h
/* OSu16tof32: *out = *in, converted through GQR3 (u16). */
static inline void OSu16tof32(u16* in, volatile f32* out) { // volatile: SDK out-parameter, xbret/xenoblade OSFastCast.h
    *out = __OSu16tof32(in);
}

// ASM-HELPER: a float store through the u8 quantisation register (psq_st has no C spelling); the RVL SDK
// OSFastCast.h form (xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h)
static inline u8 __OSf32tou8(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    u8 r;

    asm { psq_st arg, 0(ptr), 1, OS_FASTCAST_U8 }

    r = *(u8*)ptr;
    return r;
}

// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h
/* OSf32tou8: *out = *in, converted through GQR2 (u8). */
static inline void OSf32tou8(f32* in, volatile u8* out) { // volatile: SDK out-parameter, xbret/xenoblade OSFastCast.h
    *out = __OSf32tou8(*in);
}

// ASM-HELPER: a float store through the u16 quantisation register (psq_st has no C spelling); the RVL SDK
// OSFastCast.h form (xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h)
static inline u16 __OSf32tou16(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    u16 r;

    asm { psq_st arg, 0(ptr), 1, OS_FASTCAST_U16 }

    r = *(u16*)ptr;
    return r;
}

// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h
/* OSf32tou16: *out = *in, converted through GQR3 (u16). */
static inline void OSf32tou16(f32* in, volatile u16* out) { // volatile: SDK out-parameter, xbret/xenoblade OSFastCast.h
    *out = __OSf32tou16(*in);
}

// ASM-HELPER: a float store through the s16 quantisation register (psq_st has no C spelling); the RVL SDK
// OSFastCast.h form (xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h)
static inline s16 __OSf32tos16(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    s16 r;

    asm { psq_st arg, 0(ptr), 1, OS_FASTCAST_S16 }

    r = *(s16*)ptr;
    return r;
}

// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/os/OSFastCast.h
/* OSf32tos16: *out = *in, converted through GQR5 (s16). */
static inline void OSf32tos16(f32* in, volatile s16* out) { // volatile: SDK out-parameter, xbret/xenoblade OSFastCast.h
    *out = __OSf32tos16(*in);
}

#ifdef __cplusplus
}
#endif

#endif
