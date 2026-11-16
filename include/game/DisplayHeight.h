#ifndef GAME_DISPLAYHEIGHT_H
#define GAME_DISPLAYHEIGHT_H

// unit name unconfirmed: header name invented; it holds the per-unit display-height pair.

#include <game/Display.h>

/*
 * The display height as a float and as a line count. These are statics, so every unit that
 * includes this header gets its own pair, set by its static initialiser and never read again
 * (the pair opens about 76 game static initialisers). Names invented.
 */
static float displayHeightF = GetDisplayHeight();
static int   displayHeight  = lbl_8052861C;

#endif
