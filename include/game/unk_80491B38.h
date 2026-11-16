#ifndef GAME_UNK_80491B38_H
#define GAME_UNK_80491B38_H

#include <game/CPlayer.h>

class CPlayerCostume;
struct unk_RwCamera;    // the camera (RwCamera in the RenderWare 3.7 SDK); name invented

/* The player record lbl_80491B38 (size 0x30); only the fields read here are known. Name invented. */
struct unk_80491B38 {
    u8              field_0x0[0x4];
    float           field_0x4;      // the unscaled frame time
    float           field_0x8;      // the frame time
    unk_RwCamera*   field_0xC;      // the camera: CDrawingTest uses its view matrix (0x20) and frame (0x4)
    u8              field_0x10[0x4];
    CPlayer*        field_0x14;     // the player
    CPlayerCostume* field_0x18;     // the player's costume behaviour while one exists, else NULL
    u32             field_0x1C;     // nonzero: CSceneObject refits its bounds on the frame message
    u8              field_0x20[0x10];
};

// defined elsewhere, unit not yet reconstructed; read by CAmbientInterferencePoint, CPsycologyZoneTrigger,
// CPadRumbleTrigger, CSlowMoTrigger and CSplashScreenTrigger; CPlayerCostume sets and clears field_0x18
extern unk_80491B38 lbl_80491B38;

/*
 * The player record. Name invented; CPlayerCostume's destructor needs the call (MWCC resets the
 * vtable pointers in a destructor only when its body makes a call) and the record bound to a local.
 */
inline unk_80491B38& GetPlayerRecord() {
    return lbl_80491B38;
}

#endif
