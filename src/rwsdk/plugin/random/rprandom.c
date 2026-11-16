// Unit name and boundary: Gormiti link map (RpRandom.a rprandom.obj)
// Names, locals and statement order: Gormiti link map and DWARF (RpRandom.a rprandom.obj).
/*
 * rprandom.c: the RpRandom plugin. Each engine instance keeps a lagged Fibonacci generator, the
 * additive feedback generator of BSD random() with a state of 31 words and a lag of 3: the state
 * is allocated when the engine opens and seeded with a fixed value, and RpRandom hands out its
 * 31-bit numbers. RpRandomSeed has no copy in retail (inlined into RandomOpen).
 */
#include <rwsdk/badevice.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plugin/random/rprandom.h>

/* The words of the generator state, and the lag of its two pointers. Names invented. */
#define RANDOM_DEGREE 31
#define RANDOM_SEPARATION 3

/* The seed the generator starts with, and the numbers it discards after seeding. Names invented. */
#define RANDOM_DEFAULT_SEED 0x9A319039U
#define RANDOM_WARMUP 310

/* The multiplier and increment that spread a seed over the state (the ANSI C rand() constants). */
#define RANDOM_MULTIPLIER 1103515245U
#define RANDOM_INCREMENT 12345U

/*
 * RandomGlobals: the plugin's part of the engine instance. fptr and rptr walk the state, RANDOM_SEPARATION
 * words apart; endPtr is the end of the state. Type and member names invented.
 */
typedef struct RandomGlobals RandomGlobals;
struct RandomGlobals {
    RwUInt32* state;    /* 0x0: the RANDOM_DEGREE words of the generator */
    RwUInt32* fptr;     /* 0x4 */
    RwUInt32* rptr;     /* 0x8 */
    RwUInt32* endPtr;   /* 0xC */
};

static RwInt32 GlobalOffset;

#define RANDOMGLOBAL(var) (RWPLUGINOFFSET(RandomGlobals, RwEngineInstance, GlobalOffset)->var)

/*
 * ==================== RandomClose ====================
 * Frees the generator state when the engine closes.
 */
static void* RandomClose(void* instance, RwInt32 offset, RwInt32 size) {
    if (RANDOMGLOBAL(state) != NULL) {
        RwFree(RANDOMGLOBAL(state));
        RANDOMGLOBAL(state) = NULL;
    }

    return instance;
} // 0x802C7EF4

/*
 * ==================== RandomOpen ====================
 * Allocates the generator state when the engine opens and seeds it.
 */
static void* RandomOpen(void* instance, RwInt32 offset, RwInt32 size) {
    RANDOMGLOBAL(state) = (RwUInt32*)RwMalloc(RANDOM_DEGREE * sizeof(RwUInt32), rwMEMHINTDUR_GLOBAL | rwID_RANDOMPLUGIN);
    if (RANDOMGLOBAL(state) == NULL) {
        return NULL;
    }

    RANDOMGLOBAL(fptr) = RANDOMGLOBAL(state) + RANDOM_SEPARATION;
    RANDOMGLOBAL(rptr) = RANDOMGLOBAL(state);
    RANDOMGLOBAL(endPtr) = RANDOMGLOBAL(state) + RANDOM_DEGREE;

    RpRandomSeed(RANDOM_DEFAULT_SEED);

    return instance;
} // 0x802C7F50

/*
 * ==================== RpRandomSeed ====================
 * Seeds the generator: the state is filled by a linear congruential generator started at the
 * seed, then the first RANDOM_WARMUP numbers are thrown away.
 */
void RpRandomSeed(RwUInt32 seed) {
    int i;

    RANDOMGLOBAL(state)[0] = seed;
    for (i = 1; i < RANDOM_DEGREE; i++) {
        RANDOMGLOBAL(state)[i] = RANDOM_MULTIPLIER * RANDOMGLOBAL(state)[i - 1] + RANDOM_INCREMENT;
    }

    RANDOMGLOBAL(fptr) = RANDOMGLOBAL(state) + RANDOM_SEPARATION;
    RANDOMGLOBAL(rptr) = RANDOMGLOBAL(state);

    for (i = 0; i < RANDOM_WARMUP; i++) {
        RpRandom();
    }
}

/*
 * ==================== RpRandom ====================
 * Returns the next random number, of 31 bits: adds the word at rptr to the word at fptr and
 * advances both pointers around the state.
 */
RwUInt32 RpRandom(void) {
    int i;

    *RANDOMGLOBAL(fptr) += *RANDOMGLOBAL(rptr);
    i = *RANDOMGLOBAL(fptr) >> 1;

    if (++RANDOMGLOBAL(fptr) >= RANDOMGLOBAL(endPtr)) {
        RANDOMGLOBAL(fptr) = RANDOMGLOBAL(state);
        RANDOMGLOBAL(rptr)++;
    } else {
        if (++RANDOMGLOBAL(rptr) >= RANDOMGLOBAL(endPtr)) {
            RANDOMGLOBAL(rptr) = RANDOMGLOBAL(state);
        }
    }

    return i;
} // 0x802C81B8

/*
 * ==================== RpRandomPluginAttach ====================
 * Registers the plugin with the engine. Returns FALSE when the engine has no room for it.
 */
RwBool RpRandomPluginAttach(void) {
    GlobalOffset = RwEngineRegisterPlugin(sizeof(RandomGlobals), rwID_RANDOMPLUGIN, RandomOpen, RandomClose);
    if (GlobalOffset < 0) {
        return FALSE;
    }

    return TRUE;
} // 0x802C826C
