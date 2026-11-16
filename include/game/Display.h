#ifndef GAME_DISPLAY_H
#define GAME_DISPLAY_H

/*
 * Display dimensions (.sdata 0x80528618-0x80528620; the owning unit is not yet reconstructed).
 * Header name invented.
 */

extern int lbl_8052861C;    // 528: display height in lines

// .sdata 0x80527E50, owning unit not yet reconstructed: a display flag (role unknown; while it is
// set, CDrawingTest draws its painting at half brightness)
extern bool lbl_80527E50;

/*
====================
GetDisplayHeight

Name invented. Returns the height through a const reference; the per-unit pair in
game/DisplayHeight.h reads one of its two copies through it (docs/exceptions.md,
"Display-height accessor").
====================
*/
inline const int& GetDisplayHeight() {
    return lbl_8052861C;
}

#endif
