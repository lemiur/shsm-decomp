// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/R4QK01/HBMCommon.cpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMCommon.cpp
#include "HBMCommon.h"

#include <revolution/hbm.h>
#include <revolution/mem/expHeap.h>
#include <revolution/os.h>

#include "HBMAxSound.h"
#include "HBMBase.h"
#include "nw4hbm/lyt/layout.h"

const char* __HBMVersion = "<< RVL_SDK - HBM \trelease build: Mar  3 2009 14:00:03 (0x4201_142) >>";

static MEMAllocator sAllocator;
MEMAllocator* spAllocator = &sAllocator;

/*
 * HBMAllocMem
 */
void* HBMAllocMem(u32 size) {
    return MEMAllocFromAllocator(spAllocator, size);
} // 0x803B8750

/*
 * HBMFreeMem
 */
void HBMFreeMem(void* mem) {
    MEMFreeToAllocator(spAllocator, mem);
} // 0x803B8760

/* Where the menu's memory comes from. */
enum HBMAllocatorType {
    HBM_ALLOCATOR_APPLI,  // the application's allocator (HBMDataInfo::pAllocator)
    HBM_ALLOCATOR_LOCAL,  // an expanded heap on the work memory (HBMDataInfo::mem)
    HBM_ALLOCATOR_NW4HBM  // the allocator already set for nw4hbm layouts
};

/*
 * getAllocatorType
 */
static HBMAllocatorType getAllocatorType(const HBMDataInfo* pHBInfo) {
    if (pHBInfo->pAllocator) {
        return HBM_ALLOCATOR_APPLI;
    } else if (pHBInfo->mem) {
        return HBM_ALLOCATOR_LOCAL;
    } else {
        return HBM_ALLOCATOR_NW4HBM;
    }
} // no copy in retail (inlined)

/*
 * HBMCreate
 *
 * Sets up the allocator (and, for work memory, an expanded heap on it), shares
 * it with the nw4hbm layouts and creates the menu.
 */
void HBMCreate(const HBMDataInfo* pHBInfo) {
    MEMHeapHandle hExpHeap;

    if (getAllocatorType(pHBInfo) == HBM_ALLOCATOR_LOCAL) {
        hExpHeap = MEMCreateExpHeap(pHBInfo->mem, pHBInfo->memSize);
        MEMInitAllocatorForExpHeap(&sAllocator, hExpHeap, 32);
        spAllocator = &sAllocator;
    }

    switch (getAllocatorType(pHBInfo)) {
    case HBM_ALLOCATOR_APPLI:
        nw4hbm::lyt::Layout::SetAllocator(pHBInfo->pAllocator);
        spAllocator = pHBInfo->pAllocator;
        break;

    case HBM_ALLOCATOR_LOCAL:
        nw4hbm::lyt::Layout::SetAllocator(spAllocator);
        break;

    case HBM_ALLOCATOR_NW4HBM:
        spAllocator = nw4hbm::lyt::Layout::GetAllocator();
        break;
    }

    homebutton::HomeButton::createInstance(pHBInfo);
    homebutton::HomeButton::getInstance()->create();
} // 0x803B8770

/*
 * HBMDelete
 *
 * Deletes the menu, then the local heap if HBMCreate made one.
 */
void HBMDelete(void) {
    const HBMDataInfo* pHBInfo = homebutton::HomeButton::getInstance()->getHBMDataInfo();

    homebutton::HomeButton::deleteInstance();
    if (getAllocatorType(pHBInfo) == HBM_ALLOCATOR_LOCAL) {
        MEMDestroyExpHeap(spAllocator->pHeap);
    }
} // 0x803B8890

/*
 * HBMInit
 */
void HBMInit(void) {
    OSRegisterVersion(__HBMVersion);
    homebutton::HomeButton::getInstance()->init();
} // 0x803B8900

/*
 * HBMCalc
 */
HBMSelectBtnNum HBMCalc(const HBMControllerData* pController) {
    homebutton::HomeButton::getInstance()->calc(pController);
    return homebutton::HomeButton::getInstance()->getSelectBtnNum();
} // 0x803B8930

/*
 * HBMDraw
 */
void HBMDraw(void) {
    homebutton::HomeButton::getInstance()->draw();
} // 0x803B8970

/*
 * HBMGetSelectBtnNum
 */
HBMSelectBtnNum HBMGetSelectBtnNum(void) {
    return homebutton::HomeButton::getInstance()->getSelectBtnNum();
} // 0x803B89A0

/*
 * HBMSetAdjustFlag
 */
void HBMSetAdjustFlag(BOOL flag) {
    homebutton::HomeButton::getInstance()->setAdjustFlag(flag);
} // 0x803B89D0

/*
 * HBMCreateSound
 */
void HBMCreateSound(void* soundData, void* memBuf, u32 memSize) {
    homebutton::HomeButton::getInstance()->createSound(soundData, memBuf, memSize);
} // 0x803B8A10

/*
 * HBMDeleteSound
 */
void HBMDeleteSound(void) {
    homebutton::ShutdownAxSound();
} // 0x803B8A70

/*
 * HBMUpdateSound
 */
void HBMUpdateSound(void) {
    homebutton::HomeButton::getInstance()->update_sound();
} // 0x803B8A80
