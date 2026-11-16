#ifndef GAME_MESSAGES_H
#define GAME_MESSAGES_H

#include <climax/unk_801A92A8.h>

/*
 * Engine-wide messages that several behaviours subscribe to (.sbss 0x8052A780-0x8052A7A0;
 * the owning unit is not yet reconstructed). Header name invented.
 */

extern unk_801A92A8 lbl_8052A780;   // the pad manager (unk_8000A040) and unk_800C8598 listen to it
extern unk_801A92A8 lbl_8052A788;   // CAmbientInterferencePoint listens to it each frame
extern unk_801A92A8 lbl_8052A790;   // CCollisionCapsule listens to it and aligns its body
extern unk_801A92A8 lbl_8052A798;   // the pad manager listens to it
extern unk_801A92A8 lbl_8052A7A0;   // CFadeTrigger listens to it, with lbl_8052A788, while its fade runs

// .sbss 0x8052A828, owner not yet reconstructed: sent once per frame for 2D drawing; the splash
// screen fades in on it and CDesignerDebugMsg draws its text on it. Role name invented.
extern unk_801A92A8 lbl_8052A828;

// .sbss 0x80529F18, owner not yet reconstructed: the combat zone listens to it for the enemy count
// that CCombatZoneSetEnemyCountTrigger posts. Role name invented.
extern unk_801A92A8 lbl_80529F18;

// .sbss 0x80529E70, owner not yet reconstructed: CScreenShakeTrigger posts its shake to it. Role name invented.
extern unk_801A92A8 lbl_80529E70;

// .sbss 0x805294E0, owner not yet reconstructed: CPlayerStateTrigger posts its state name and
// flag to it. Role name invented.
extern unk_801A92A8 lbl_805294E0;

#endif
