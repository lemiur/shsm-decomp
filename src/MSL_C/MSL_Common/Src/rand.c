// Reference: xbret/xenoblade libs/PowerPC_EABI_Support/src/MSL_C/MSL_Common/rand.c
/* Unit name unconfirmed: MSL rand.c, as in the public decomps; no DOL string names the file. */

#include <stdlib.h>

static unsigned int random_next = 1;

/*
 * rand
 *
 * The ANSI C linear congruential generator: advances the seed and returns
 * bits 16-30 of it, 0 to 0x7FFF.
 */
int rand(void) {
    random_next = random_next * 1103515245 + 12345;
    return (random_next >> 16) & 0x7fff;
} // 0x8019C058

/*
 * srand
 *
 * Restarts the sequence of rand from the given seed.
 */
void srand(unsigned int seed) {
    random_next = seed;
} // 0x8019C078
