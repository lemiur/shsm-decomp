#include <revolution/os.h>

/* Rounds n up to a multiple of a, which must be a power of two. */
#define ROUND(n, a) (((u32)(n) + (a) - 1) & ~((a) - 1))

static void* __OSArenaHi = NULL;
static void* s_mem2ArenaHi = NULL;

/* -1 marks an arena bound that OSInit has not set yet. */
static void* __OSArenaLo = (void*)-1;
static void* s_mem2ArenaLo = (void*)-1;

void* OSGetMEM1ArenaHi(void) {
    return __OSArenaHi;
} // 0x802129C0

void* OSGetMEM2ArenaHi(void) {
    return s_mem2ArenaHi;
} // 0x802129D0

void* OSGetArenaHi(void) {
    return __OSArenaHi;
} // 0x802129E0

void* OSGetMEM1ArenaLo(void) {
    return __OSArenaLo;
} // 0x802129F0

void* OSGetMEM2ArenaLo(void) {
    return s_mem2ArenaLo;
} // 0x80212A00

void* OSGetArenaLo(void) {
    return __OSArenaLo;
} // 0x80212A10

void OSSetMEM1ArenaHi(void* newHi) {
    __OSArenaHi = newHi;
} // 0x80212A20

void OSSetMEM2ArenaHi(void* newHi) {
    s_mem2ArenaHi = newHi;
} // 0x80212A30

void OSSetArenaHi(void* newHi) {
    __OSArenaHi = newHi;
} // 0x80212A40

void OSSetMEM1ArenaLo(void* newLo) {
    __OSArenaLo = newLo;
} // 0x80212A50

void OSSetMEM2ArenaLo(void* newLo) {
    s_mem2ArenaLo = newLo;
} // 0x80212A60

void OSSetArenaLo(void* newLo) {
    __OSArenaLo = newLo;
} // 0x80212A70

/*
 * OSAllocFromMEM1ArenaLo
 *
 * Carves an aligned block off the bottom of the MEM1 arena.
 */
void* OSAllocFromMEM1ArenaLo(u32 size, u32 align) {
    void* ptr;
    u8* arenaLo;

    ptr = OSGetMEM1ArenaLo();
    arenaLo = ptr = (void*)ROUND(ptr, align);
    arenaLo += size;
    arenaLo = (u8*)ROUND(arenaLo, align);
    OSSetMEM1ArenaLo(arenaLo);

    return ptr;
} // 0x80212A80
