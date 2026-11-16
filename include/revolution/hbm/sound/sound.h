#ifndef HBM_SOUND_SOUND_H
#define HBM_SOUND_SOUND_H

// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/sound/*.cpp (ARRAY_LENGTH)
//
// The HBM sound units size their fixed tables, and their loops over those tables, with ARRAY_LENGTH.
// The reference project keeps the helper in its decomp.h, which this project has no equivalent of, so
// the sound units share it here.

/* The number of elements of a fixed-size array. */
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

#endif
