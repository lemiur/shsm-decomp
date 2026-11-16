#ifndef RWSDK_PLUGIN_RANDOM_RPRANDOM_H
#define RWSDK_PLUGIN_RANDOM_RPRANDOM_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seeds the Mersenne Twister generator (no copy in retail). */
void RpRandomSeedMT(RwUInt32 seed);

/* Returns the next 32-bit random number of the Mersenne Twister generator. */
RwUInt32 RpRandomMT(void);

/* Seeds the lagged Fibonacci generator (no copy in retail: inlined into the engine open callback). */
void RpRandomSeed(RwUInt32 seed);

/* Returns the next 31-bit random number of the lagged Fibonacci generator. */
RwUInt32 RpRandom(void);

/* Registers the plugin with the engine. */
RwBool RpRandomPluginAttach(void);

#ifdef __cplusplus
}
#endif

#endif
