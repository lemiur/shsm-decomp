// Unit name and boundary: Gormiti link map (RpRandom.a mertwist.obj)
// Names, locals and statement order: Gormiti link map and DWARF (RpRandom.a mertwist.obj).
/*
 * mertwist.c: the random number generator of the RpRandom plugin, the Mersenne Twister MT19937
 * in Shawn Cokus's formulation (mt19937ar-cok): the state is reloaded 624 words at a time and
 * its words are tempered as they are handed out. The generator seeds itself with 4357 when it is
 * used before RpRandomSeedMT (no copy in retail, dead-stripped) set it. Seeding and reloading
 * are open-coded in RpRandomMT.
 */
#include <rwsdk/rwtypes.h>
#include <rwsdk/plugin/random/rprandom.h>

#define N 624
#define M 397
#define K 0x9908B0DFU
#define MT_DEFAULT_SEED 4357U         /* the seed used when none was set */
#define MT_LCG_MULTIPLIER 69069U      /* the multiplier of the generator that fills the state */
#define MT_WORD_MASK 0xFFFFFFFFU
#define MT_TEMPER_B 0x9D2C5680U       /* the tempering masks */
#define MT_TEMPER_C 0xEFC60000U
#define hiBit(u) ((u) & 0x80000000U)
#define loBit(u) ((u) & 0x00000001U)
#define loBits(u) ((u) & 0x7FFFFFFFU)
#define mixBits(u, v) (hiBit(u) | loBits(v))

static RwUInt32 state[N + 1];
static RwUInt32* next;
static int left = -1;

/*
 * ==================== RpRandomMT ====================
 * Returns the next 32-bit random number. When the state is used up it is regenerated first (and
 * seeded with 4357 if nothing seeded it).
 */
RwUInt32 RpRandomMT(void) {
    RwUInt32 result;

    if (--left < 0) {
        RwUInt32* p0 = state;
        RwUInt32* p2 = state + 2;
        RwUInt32* pM = state + M;
        RwUInt32 s0;
        RwUInt32 s1;
        int j;

        if (left < -1) {
            RwUInt32 x = (MT_DEFAULT_SEED | 1U) & MT_WORD_MASK;
            RwUInt32* s = state;
            int j;

            for (left = 0, *s++ = x, j = N; --j; *s++ = (x *= MT_LCG_MULTIPLIER) & MT_WORD_MASK) {
            }
        }

        left = N - 1;
        next = state + 1;

        for (s0 = state[0], s1 = state[1], j = N - M + 1; --j; s0 = s1, s1 = *p2++) {
            *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
        }

        for (pM = state, j = M; --j; s0 = s1, s1 = *p2++) {
            *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
        }

        s1 = state[0];
        *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);

        s1 ^= (s1 >> 11);
        s1 ^= (s1 << 7) & MT_TEMPER_B;
        s1 ^= (s1 << 15) & MT_TEMPER_C;
        result = s1 ^ (s1 >> 18);
    } else {
        result = *next++;
        result ^= (result >> 11);
        result ^= (result << 7) & MT_TEMPER_B;
        result ^= (result << 15) & MT_TEMPER_C;
        result ^= (result >> 18);
    }

    return result;
} // 0x802C7D1C
