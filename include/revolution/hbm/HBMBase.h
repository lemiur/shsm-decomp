#ifndef HBM_BASE_H
#define HBM_BASE_H

// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/HBMBase.h
// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/homebutton/R4QK01/HBMBase.hpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMBase.h

#include <revolution/types.h>
#include <revolution/ax.h>
#include <revolution/axfx.h>
#include <revolution/wpad.h>
#include <revolution/wpad/__wpad.h>

#include "HBMCommon.h"
#include "HBMAxSound.h"
#include "HBMController.h"
#include "HBMGUIManager.h"
#include "nw4hbm/lyt/drawInfo.h"
#include "nw4hbm/lyt/layout.h"

struct HBMDataInfoEx;

namespace nw4hbm {
namespace lyt {
class ArcResourceAccessor;
} // namespace lyt
} // namespace nw4hbm

namespace homebutton {

class HomeButton;
class GroupAnmController;

/*
 * HomeButtonEventHandler
 *
 * Hands the menu's pane events back to the menu itself.
 */
class HomeButtonEventHandler : public gui::EventHandler {
public:
    HomeButtonEventHandler(homebutton::HomeButton* pHomeButton) :
        mpHomeButton(pHomeButton) {}

    /* 0x08 */ virtual void onEvent(u32 uID, u32 uEvent, void* pData);

    homebutton::HomeButton* getHomeButton() { return mpHomeButton; }

private:
    /* 0x00 (base) */
    /* 0x08 */ HomeButton* mpHomeButton;
}; // size = 0x0C

/*
 * HomeButton
 *
 * The Home Button Menu itself: the layouts, the animation controllers, the
 * controllers and the menu's own sound. One instance at a time, created by
 * HBMCreate (HBMCommon) through createInstance.
 */
class HomeButton {
private:
    /* The sequence the app has the menu in (its selected animation set). */
    typedef enum {
        /* 0 */ eSeq_Normal,
        /* 1 */ eSeq_Control,
        /* 2 */ eSeq_Cmn,
    } eSeq;

    /*
     * BlackFader
     *
     * The black plate the menu fades in and out over the application.
     */
    class BlackFader {
    public:
        BlackFader(int maxFrame) {
            init(maxFrame);
            setColor(0, 0, 0);
            flag = true;
        }

        void setColor(u8 r, u8 g, u8 b) {
            red_ = r;
            green_ = g;
            blue_ = b;
        }

        int getFrame() const { return frame_; }
        int getMaxFrame() const { return maxFrame_; }
        void start() { state_ = 1; }
        int getFadeColorEnable() const { return flag; }
        void setFadeColorEnable(bool value) { flag = value; }

        bool isDone();
        void init(int maxFrame);
        void calc();
        void draw();

    private:
        static const int scMaxAlpha;

        /* 0x00 */ int frame_;
        /* 0x04 */ int maxFrame_;
        /* 0x08 */ int state_;
        /* 0x0C */ bool flag;
        /* 0x0D */ u8 red_;
        /* 0x0E */ u8 green_;
        /* 0x0F */ u8 blue_;
    }; // size = 0x10

public:
    HomeButton(const HBMDataInfo* dataInfo);
    ~HomeButton();

    int getVolume();
    HBMSelectBtnNum getSelectBtnNum();
    bool isActive() const;
    bool isUpBarActive() const;
    bool isDownBarActive();
    void setAdjustFlag(int flag);
    void setForcusSE();
    void setSimpleSyncAlarm(int type);
    void setSpeakerAlarm(int chan, int msec);
    void setVolume(int vol);
    bool getVibFlag();
    int getPaneNo(const nw4hbm::lyt::Pane*);
    void setVibFlag(bool flag);
    void create();
    void init();
    void calc(const HBMControllerData* pController);
    void draw();
    void update(const HBMControllerData* pController);
    void updateTrigPane();
    void startPointEvent(const nw4hbm::lyt::Pane* pPane, void* pData);
    void startLeftEvent(const nw4hbm::lyt::Pane* pPane);
    void startTrigEvent(const nw4hbm::lyt::Pane* pPane);
    int findAnimator(int pane, int anm);
    int findGroupAnimator(int pane, int anm);
    void callSimpleSyncCallback(s32 result, s32 num);
    void startBlackOut();

    const HBMDataInfo* getHBMDataInfo() { return mpHBInfo; }
    Controller* getController(int chan) { return mpController[chan]; }
    const char* getFuncPaneName(int no) { return scFuncTouchPaneName[no]; }
    const char* getPaneName(int no) { return scBtnName[no]; }
    bool getReassignedFlag() const { return mReassignedFlag; }
    HomeButtonEventHandler* getEventHandler() const { return mpHomeButtonEventHandler; }
    void setEndSimpleSyncFlag(bool flag) { mEndSimpleSyncFlag = flag; }
    void setReassignedFlag(bool flag) { mReassignedFlag = flag; }
    void setSimpleSyncFlag(bool flag) { mSimpleSyncFlag = flag; }

    static void createInstance(const HBMDataInfo* dataInfo);
    static HomeButton* getInstance();
    static void deleteInstance();

private:
    void init_battery(const HBMControllerData* pController);
    void calc_battery(int chan);
    void reset_battery();

    void init_sound();
    void fadeout_sound(f32 gain);

    void init_msg();
    void init_vib();
    void init_volume();

    void set_config();
    void set_text();

    void calc_fadeoutAnm();

    void update_controller(int id);
    void update_posController(int id);

    void reset_btn();
    void reset_control();
    void reset_guiManager(int num);
    void reset_window();

public:
    void play_sound(int id);
    void createSound(const void* soundData, void* work, u32 workSize) { InitAxSound(soundData, work, workSize); }
    void update_sound();

private:
    /* 0x000 */ eSeq mSequence;
    /* 0x004 */ const HBMDataInfo* mpHBInfo;
    /* 0x008 */ const HBMDataInfoEx* mpHBInfoEx;
    /* 0x00C */ int mButtonNum;
    /* 0x010 */ int mAnmNum;
    /* 0x014 */ int mState;
    /* 0x018 */ int mSelectAnmNum;
    /* 0x01C */ int mMsgCount;
    /* 0x020 */ int mPaneCounter[14];
    /* 0x058 */ int mPadDrawTime[WPAD_MAX_CONTROLLERS];
    /* 0x068 */ int mForcusSEWaitTime;
    /* 0x06C */ int mBar0AnmRev;
    /* 0x070 */ int mBar1AnmRev;
    /* 0x074 */ int mBar0AnmRevHold;
    /* 0x078 */ int mBar1AnmRevHold;
    /* 0x07C */ int mGetPadInfoTime;
    /* 0x080 */ bool mControllerFlag[WPAD_MAX_CONTROLLERS];
    /* 0x084 */ int mVolumeNum;
    /* 0x088 */ int mConnectNum;
    /* 0x08C */ bool mVibFlag;
    /* 0x08D */ bool mControlFlag;
    /* 0x08E */ bool mLetterFlag;
    /* 0x08F */ bool mAdjustFlag;
    /* 0x090 */ bool mReassignedFlag;
    /* 0x091 */ bool mSimpleSyncFlag;
    /* 0x092 */ bool mEndSimpleSyncFlag;
    /* 0x093 */ bool mForthConnectFlag;
    /* 0x094 */ bool mInitFlag;
    /* 0x095 */ bool mForceSttInitProcFlag;
    /* 0x096 */ bool mForceSttFadeInProcFlag;
    /* 0x097 */ bool mEndInitSoundFlag;
    /* 0x098 */ bool mForceStopSyncFlag;
    /* 0x099 */ bool mForceEndMsgAnmFlag;
    /* 0x09A */ bool mStartBlackOutFlag;
    /* 0x09C */ int mSoundRetryCnt;
    /* 0x0A0 */ int mDialogFlag[4];
    /* 0x0B0 */ char* mpLayoutName;
    /* 0x0B4 */ char* mpAnmName;
    /* 0x0B8 */ HBMSelectBtnNum mSelectBtnNum;
    /* 0x0BC */ wchar_t* mpText[10][6];
    /* 0x1AC */ WPADInfo mWpadInfo[WPAD_MAX_CONTROLLERS];
    /* 0x20C */ WPADSyncDeviceCallback mSimpleSyncCallback;
    /* 0x210 */ f32 mOnPaneVibFrame[4];
    /* 0x220 */ f32 mOnPaneVibWaitFrame[4];
    /* 0x230 */ int mWaitStopMotorCount;
    /* 0x234 */ int mDisConnectCount;
    /* 0x238 */ nw4hbm::lyt::Layout* mpLayout;
    /* 0x23C */ nw4hbm::lyt::Layout* mpCursorLayout[WPAD_MAX_CONTROLLERS];
    /* 0x24C */ nw4hbm::lyt::ArcResourceAccessor* mpResAccessor;
    /* 0x250 */ gui::PaneManager* mpPaneManager;
    /* 0x254 */ HomeButtonEventHandler* mpHomeButtonEventHandler;
    /* 0x258 */ nw4hbm::lyt::DrawInfo mDrawInfo;
    /* 0x2AC */ Controller* mpController[WPAD_MAX_CONTROLLERS];
    /* 0x2BC */ RemoteSpk* mpRemoteSpk;
    /* 0x2C0 */ GroupAnmController* mpAnmController[12];
    /* 0x2F0 */ GroupAnmController* mpGroupAnmController[74];
    /* 0x418 */ GroupAnmController* mpPairGroupAnmController[15];
    /* 0x454 */ BlackFader mFader;
    /* 0x468 */ OSAlarm mAlarm[WPAD_MAX_CONTROLLERS];
    /* 0x528 */ OSAlarm mSpeakerAlarm[WPAD_MAX_CONTROLLERS];
    /* 0x5E8 */ OSAlarm mSimpleSyncAlarm;
    /* 0x618 */ int iVISetBlackFlag;
    /* 0x61C */ int iReConnectTime;
    /* 0x620 */ int iReConnectTime2;
    /* 0x624 */ u16 mAppVolume[3];
    /* 0x62C */ AXFXAllocHook mAxFxAlloc;
    /* 0x630 */ AXFXFreeHook mAxFxFree;
    /* 0x634 */ AXFX_REVERBHI mAxFxReverb;
    /* 0x794 */ AXAuxCallback mAuxCallback;
    /* 0x798 */ void* mpAuxContext;
    /* 0x79C */ f32 mFadeOutSeTime;

    // static members
private:
    static const int scReConnectTime;
    static const int scReConnectTime2;
    static const int scPadDrawWaitTime;
    static const int scGetPadInfoTime;
    static const int scForcusSEWaitTime;
    static const f32 scOnPaneVibTime;
    static const f32 scOnPaneVibWaitTime;
    static const int scWaitStopMotorTime;
    static const int scWaitDisConnectTime;
    static const int scSoundHeapSize_but2;
    static const int scSoundHeapSize_but3;
    static const int scSoundThreadPrio;
    static const int scDvdThreadPrio;

    static HomeButton* spHomeButtonObj;
    static OSMutex sMutex;

    static const char* scCursorLytName[WPAD_MAX_CONTROLLERS];
    static const char* scCursorPaneName;
    static const char* scCursorRotPaneName;
    static const char* scCursorSRotPaneName;

    static const char* scBtnName[4];
    static const char* scTxtName[4];
    static const char* scGrName[8];
    static const char* scAnimName[3];
    static const char* scPairGroupAnimName[15];
    static const char* scPairGroupName[15];
    static const char* scGroupAnimName[22];
    static const char* scGroupName[35];
    static const char* scFuncPaneName[5];
    static const char* scFuncTouchPaneName[10];
    static const char* scFuncTextPaneName[3];
    static const char* scBatteryPaneName[WPAD_MAX_CONTROLLERS][4];
}; // size = 0x7A0

} // namespace homebutton

#endif
