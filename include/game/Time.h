#ifndef GAME_TIME_H
#define GAME_TIME_H

// Header name unconfirmed (invented).

// defined in 0x80202384, unit not yet reconstructed: sets the game's time scale and the time
// taken to blend to it. Signature and name invented; called by CScreenShakeTrigger and CSlowMoTrigger.
void fn_80202384( float scale, float blendTime );

#endif
