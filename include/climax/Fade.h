#ifndef CLIMAX_FADE_H
#define CLIMAX_FADE_H

#include <climax/Colour.h>

/*
 * The screen fade of the Climax engine (0x801E2DA0 and 0x801E31C4, unit not yet reconstructed).
 * Header and names invented.
 */

struct unk_ScreenFade;                  // the fade state: level, rate, colour

extern unk_ScreenFade* lbl_8052A678;    // the screen fade

// defined in 0x801E2DA0, unit not yet reconstructed: nonzero while the screen fade runs
bool    fn_801E2DA0( int arg0 );
// defined in 0x801E31C4, unit not yet reconstructed: starts a fade at rate (level per second; negative
// rates fall) towards colour; the meaning of the flag is unknown
void    fn_801E31C4( unk_ScreenFade* fade, float rate, const unk_RGBA* colour, bool flag );

#endif
