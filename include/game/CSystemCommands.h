#ifndef GAME_CSYSTEMCOMMANDS_H
#define GAME_CSYSTEMCOMMANDS_H

// unit name unconfirmed: header name invented, after the class name the hash is taken from.

#include <revolution/types.h>

/*
 * The class id of the system attribute commands, hashed from "CSystemCommands" by the static
 * initialiser 0x800A2900 (unit not yet reconstructed). Behaviours read these commands as well as
 * their own.
 */
extern u32 lbl_805297E8;

/* System attribute commands. Names invented. */
enum {
    SYSCMD_SetMatrix = 1    // presumed from use: PlaneTrigger rebuilds its planes from the frame on it
};

#endif
