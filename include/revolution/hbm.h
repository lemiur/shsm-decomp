#ifndef REVOLUTION_HBM_H
#define REVOLUTION_HBM_H

// Reference: xbret/xenoblade libs/RVL_SDK/include/revolution/hbm/HBMTypes.h
// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/HBMCommon.h

#include <revolution/types.h>
#include <revolution/kpad.h>
#include <revolution/mem/allocator.h>
#include <revolution/mtx.h>
#include <revolution/wpad.h>

/* Work memory HBMCreateSound needs for the menu's own sound (sound data, sequence players, the
 * audio thread and its stack). */
#define HBM_MEM_SIZE_SOUND 0x18700

#ifdef __cplusplus
extern "C" {
#endif

/* The button the player chose in the menu, returned by HBMCalc. */
typedef enum HBMSelectBtnNum {
    HBM_SELECT_NULL = -1,

    HBM_SELECT_HOMEBTN,
    HBM_SELECT_BTN1,
    HBM_SELECT_BTN2,
    HBM_SELECT_BTN3,
    HBM_SELECT_BTN4,

    HBM_SELECT_MAX
} HBMSelectBtnNum;

/* Events passed to the application's sound callback. */
typedef enum HBMSoundEvent {
    HBM_SOUND_INIT,
    HBM_SOUND_POST_INIT,
    HBM_SOUND_GOTO_MENU,
    HBM_SOUND_RETURN_APP,
    HBM_SOUND_STOP,
    HBM_SOUND_PLAY
} HBMSoundEvent;

typedef int (*HBMSoundCallback)(int event, int arg);

/*
 * HBMDataInfo
 *
 * The application's description of the menu: resource buffers, the work
 * memory or allocator, the sound callback and display options.
 */
typedef struct HBMDataInfo {
    void* layoutBuf;                 // 0x00
    void* spkSeBuf;                  // 0x04
    void* msgBuf;                    // 0x08
    void* configBuf;                 // 0x0C
    void* mem;                       // 0x10: work memory for a local heap
    HBMSoundCallback sound_callback; // 0x14
    BOOL backFlag;                   // 0x18
    int region;                      // 0x1C
    BOOL cursor;                     // 0x20
    BOOL messageFlag;                // 0x24
    u32 configBufSize;               // 0x28
    u32 memSize;                     // 0x2C
    f32 frameDelta;                  // 0x30
    Vec2 adjust;                     // 0x34
    MEMAllocator* pAllocator;        // 0x3C: the application's allocator, if any
} HBMDataInfo; // size 0x40

typedef struct HBMKPadData {
    KPADStatus* kpad; // 0x00
    Vec2 pos;         // 0x04
    u32 use_devtype;  // 0x0C
} HBMKPadData; // size 0x10

typedef struct HBMControllerData {
    HBMKPadData wiiCon[WPAD_MAX_CONTROLLERS]; // 0x00
} HBMControllerData; // size 0x40

void HBMCreate(const HBMDataInfo* pHBInfo);
void HBMDelete(void);
void HBMInit(void);
HBMSelectBtnNum HBMCalc(const HBMControllerData* pController);
void HBMDraw(void);
HBMSelectBtnNum HBMGetSelectBtnNum(void);
void HBMSetAdjustFlag(BOOL flag);
void HBMStartBlackOut(void);
BOOL HBMIsReassignedControllers(void);

void HBMCreateSound(void* soundData, void* memBuf, u32 memSize);
void HBMDeleteSound(void);
void HBMUpdateSound(void);

#ifdef __cplusplus
}
#endif

#endif
