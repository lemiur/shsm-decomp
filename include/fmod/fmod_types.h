/*
 * FMOD's byte-order macros (fmod_types.h of FMOD Ex). The on-disk formats FMOD reads are
 * little-endian, so the big-endian Wii build swaps every value it reads.
 */
#ifndef _FMOD_TYPES_H
#define _FMOD_TYPES_H

#define FMOD_SWAPENDIAN_DWORD( _x ) \
    ( ( ( ( _x ) & 0x000000FF ) << 24 ) | ( ( ( _x ) & 0x0000FF00 ) << 8 ) | ( ( ( _x ) & 0x00FF0000 ) >> 8 ) | ( ( ( _x ) & 0xFF000000 ) >> 24 ) )

// ---- moved here from fmod_channeli.h for fmod_eventimpl_simple (FMOD::Global holds one) ----
// A 64-bit value the DSP clock keeps as two words; the pair is read as one number.
struct FMOD_UINT64P {
    union {
        unsigned long long mValue;
        struct {
            unsigned int mHi;
            unsigned int mLo;
        };
    };
};

// A signed 64-bit value kept as two words (Gormiti DWARF, FMOD_SINT64P: the value and its halves).
struct FMOD_SINT64P {
    union {
        long long mValue;
        struct {
            int          mHi;
            unsigned int mLo;
        };
    };
};

#endif /* _FMOD_TYPES_H */
