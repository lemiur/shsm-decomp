#ifndef HBM_CONTROLLER_H
#define HBM_CONTROLLER_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/homebutton/HBMController.hpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMController.h

#include <revolution/types.h>
#include <revolution/hbm.h>
#include <revolution/os.h>
#include <revolution/wpad.h>

#include "HBMRemoteSpk.h"

struct WPADInfo;

namespace nw4hbm {
namespace snd {
class SoundArchivePlayer;
} // namespace snd
} // namespace nw4hbm

/* The controller state the menu reads: the channel's pointer position, its buttons and its rumble flag. */
struct HBController {
    /* 0x00 */ int chan;
    /* 0x04 */ f32 spVol;
    /* 0x08 */ f32 x;
    /* 0x0C */ f32 y;
    /* 0x10 */ u32 trig;
    /* 0x14 */ u32 hold;
    /* 0x18 */ u32 release;
    /* 0x1C */ bool rumble;
}; // size 0x20

namespace homebutton {

/*
 * Controller
 *
 * One Wii Remote's menu state: the KPAD sample mapped to the menu's buttons,
 * the remote speaker, and the alarms that turn the speaker and the battery
 * polling on and off. Every live channel has one.
 */
class Controller {
public:
    Controller(int chan, RemoteSpk* spk);
    ~Controller();

    HBController* getController();
    int getChan() const;
    f32 getSpeakerVol() const;
    RemoteSpk* getRemoteSpk() const { return remotespk; }
    bool isRumbling() { return mHBController.rumble; }
    bool getBatteryFlag() const;
    void setEnableRumble(bool flag) { mRumbleFlag = flag; }

    void setSpeakerVol(f32 vol);
    void setRumble() { mHBController.rumble = true; }
    void clrRumble() { mHBController.rumble = false; }

    s32 getInfoAsync(WPADInfo* info);
    bool isPlayReady() const;
    bool isPlayingSound() const;
    bool isPlayingSoundId(int id) const;

    void setKpad(const HBMKPadData* con, bool updatePos);
    void setInValidPos();
    void clrBatteryFlag();
    void clrKpadButton();

    void connect();
    void disconnect();

    void initSound();
    void updateSound();
    void playSound(int id);
    void playSound(nw4hbm::snd::SoundArchivePlayer*, int id) { playSound(id); }

    void soundOn();
    void soundOff(int msec);

    void startMotor();
    void stopMotor();

    void initCallback();
    void clearCallback();

private:
    static void wpadConnectCallback(WPADChannel chan, WPADResult result);
    static void wpadExtensionCallback(WPADChannel chan, s32 result);
    static void soundOnCallback(OSAlarm* alm, OSContext* context);
    static void ControllerCallback(WPADChannel chan, WPADResult result);

private:
    /* 0x00 */ HBController mHBController;
    /* 0x20 */ RemoteSpk* remotespk;
    /* 0x24 */ WPADConnectCallback mOldConnectCallback;
    /* 0x28 */ WPADExtensionCallback mOldExtensionCallback;
    /* 0x30 */ OSTime mPlaySoundTime;
    /* 0x38 */ OSTime mStopSoundTime;
    /* 0x40 */ bool mCallbackFlag;
    /* 0x41 */ bool mSoundOffFlag;
    /* 0x42 */ bool mCheckSoundTimeFlag;
    /* 0x43 */ bool mCheckSoundIntervalFlag;
    /* 0x44 */ bool mRumbleFlag;

private:
    static bool sBatteryFlag[WPAD_MAX_CONTROLLERS];
    static OSAlarm sAlarm[WPAD_MAX_CONTROLLERS];
    static OSAlarm sAlarmSoundOff[WPAD_MAX_CONTROLLERS];
    static Controller* sThis[WPAD_MAX_CONTROLLERS];
}; // size 0x48

} // namespace homebutton

#endif
