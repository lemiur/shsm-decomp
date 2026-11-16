// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/HBMController.cpp (VERSION_R4QK01 branches)
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMController.cpp
#include "HBMController.h"

#include <revolution/wpad/__wpad.h>

namespace homebutton {

bool Controller::sBatteryFlag[WPAD_MAX_CONTROLLERS];
OSAlarm Controller::sAlarm[WPAD_MAX_CONTROLLERS];
OSAlarm Controller::sAlarmSoundOff[WPAD_MAX_CONTROLLERS];
Controller* Controller::sThis[WPAD_MAX_CONTROLLERS];

/*
 * getChan
 *
 * The channel the controller was created for. Every caller in the unit expands
 * it, so the object has no out-of-line copy (0x803B66F0 has none).
 */
inline int Controller::getChan() const {
    return mHBController.chan;
} // no copy in retail (inlined)

/*
 * getSpeakerVol
 *
 * The speaker volume set with setSpeakerVol (0 to 10).
 */
inline f32 Controller::getSpeakerVol() const {
    return mHBController.spVol;
} // no copy in retail (inlined)

/*
 * isPlayingSound
 *
 * Whether this channel's remote is playing a menu sound; updateSound and
 * startMotor test it.
 */
inline bool Controller::isPlayingSound() const {
    return getRemoteSpk()->isPlaying(getChan());
} // no copy in retail (inlined)

/*
 * soundOn
 *
 * Unmutes the remote speaker and clears the mute flag. Expanded into
 * soundOnCallback and updateSound.
 */
inline void Controller::soundOn() {
    int chan = getChan();

    if (WPADIsSpeakerEnabled(chan)) {
        WPADControlSpeaker(chan, WPAD_SPEAKER_UNMUTE, NULL);
    }

    mSoundOffFlag = false;
} // no copy in retail (inlined)

/*
 * soundOff
 *
 * Mutes the remote speaker, starts the alarm that sounds it again after msec,
 * and marks the speaker muted. Expanded into wpadExtensionCallback, soundOff's
 * updateSound call and the radio-quality test.
 */
inline void Controller::soundOff(int msec) {
    int chan = getChan();

    if (!WPADIsSpeakerEnabled(chan)) {
        return;
    }

    WPADControlSpeaker(chan, WPAD_SPEAKER_MUTE, NULL);

    OSSetAlarmUserDataAny(&sAlarmSoundOff[chan], chan);
    OSCancelAlarm(&sAlarmSoundOff[chan]);
    OSSetAlarm(&sAlarmSoundOff[chan], OSMillisecondsToTicks(msec), &soundOnCallback);

    mSoundOffFlag = true;
} // no copy in retail (inlined)

/*
 * wpadConnectCallback
 *
 * WPAD's connect callback: when a remote connects, switch its extension
 * callback to the menu's own and silence its speaker; when it leaves, put the
 * old callbacks back. The menu's predecessor callback runs last.
 */
void Controller::wpadConnectCallback(WPADChannel chan, WPADResult result) {
    switch (result) {
    case WPAD_ESUCCESS:
        if (!sThis[chan]->mCallbackFlag) {
            WPADSetExtensionCallback(chan, &wpadExtensionCallback);
            sThis[chan]->mCallbackFlag = true;
        }

        WPADControlSpeaker(chan, WPAD_SPEAKER_OFF, NULL);
        break;

    case WPAD_ENODEV:
        WPADSetExtensionCallback(chan, sThis[chan]->mOldExtensionCallback);
        sThis[chan]->mCallbackFlag = false;
        sThis[chan]->mCheckSoundTimeFlag = false;
        sThis[chan]->mCheckSoundIntervalFlag = false;
        break;
    }

    if (sThis[chan]->mOldConnectCallback && sThis[chan]->mOldConnectCallback != &wpadConnectCallback) {
        (*sThis[chan]->mOldConnectCallback)(chan, result);
    }
} // 0x803B66F0

/*
 * wpadExtensionCallback
 *
 * WPAD's extension callback: a Classic Controller that is still initialising
 * silences the speaker, then the menu's predecessor callback runs.
 */
void Controller::wpadExtensionCallback(WPADChannel chan, s32 result) {
    switch (result) {
    case WPAD_DEV_INITIALIZING:
        sThis[chan]->soundOff(1000);
        break;
    }

    if (sThis[chan]->mOldExtensionCallback) {
        (*sThis[chan]->mOldExtensionCallback)(chan, result);
    }
} // 0x803B6800

/*
 * soundOnCallback
 *
 * Alarm handler that sounds the speaker again after soundOff muted it.
 */
void Controller::soundOnCallback(OSAlarm* alm, OSContext*) {
    int chan = OSGetAlarmUserDataAny(int, alm);
    sThis[chan]->soundOn();
} // 0x803B6910

/*
 * Controller
 *
 * Records the channel and the remote speaker, clears the callbacks and the
 * sound flags, and creates the channel's two alarms.
 */
Controller::Controller(int chan, RemoteSpk* spk) {
    mHBController.chan = chan;
    mHBController.rumble = false;
    mHBController.spVol = 1.0f;

    remotespk = spk;
    mOldConnectCallback = NULL;
    mOldExtensionCallback = NULL;
    mCallbackFlag = false;
    mSoundOffFlag = false;
    mRumbleFlag = true;

    if (chan < WPAD_MAX_CONTROLLERS) {
        sBatteryFlag[chan] = false;
        OSCreateAlarm(&sAlarm[chan]);
        OSCreateAlarm(&sAlarmSoundOff[chan]);
        sThis[chan] = this;
    }
} // 0x803B6980

/*
 * ~Controller
 *
 * Stops the channel's alarms; the menu deletes its controllers one at a time.
 */
Controller::~Controller() {
    OSCancelAlarm(&sAlarm[mHBController.chan]);
    OSCancelAlarm(&sAlarmSoundOff[mHBController.chan]);
} // 0x803B6A40

/*
 * initCallback
 *
 * Installs the menu's WPAD callbacks and records the ones they replace, then
 * reads the connected device to set the callback flag.
 */
void Controller::initCallback() {
    WPADDeviceType type;

    WPADSetCallbackByKPAD(FALSE);
    mOldConnectCallback = WPADSetConnectCallback(mHBController.chan, &wpadConnectCallback);
    WPADSetCallbackByKPAD(TRUE);
    mOldExtensionCallback = WPADSetExtensionCallback(mHBController.chan, &wpadExtensionCallback);
    mRumbleFlag = true;

    switch (WPADProbe(mHBController.chan, &type)) {
    case WPAD_ESUCCESS:
        mCallbackFlag = true;
        break;
    case WPAD_ENODEV:
        mCallbackFlag = false;
        break;
    }
} // 0x803B6AC0

/*
 * clearCallback
 *
 * Silences the speaker and hands the channel back to the callbacks the menu
 * replaced.
 */
void Controller::clearCallback() {
    WPADControlSpeaker(mHBController.chan, WPAD_SPEAKER_ON, NULL);
    WPADSetCallbackByKPAD(FALSE);
    WPADSetConnectCallback(mHBController.chan, mOldConnectCallback);
    WPADSetCallbackByKPAD(TRUE);
    mOldConnectCallback = NULL;

    WPADSetExtensionCallback(mHBController.chan, mOldExtensionCallback);
    mOldExtensionCallback = NULL;
} // 0x803B6B60

/*
 * setKpad
 *
 * Copies a KPAD sample into the menu's controller state. The Classic
 * Controller's A, +, - and HOME buttons are translated to the remote buttons
 * the menu tests. updatePos is false while the pointer has not settled, which
 * keeps the last good position.
 */
void Controller::setKpad(const HBMKPadData* con, bool updatePos) {
    if (!con->kpad) {
        return;
    }

    if (updatePos) {
        if (con->kpad->dev_type == WPAD_DEV_CLASSIC && con->use_devtype == WPAD_DEV_CLASSIC) {
            mHBController.x = con->pos.x;
            mHBController.y = con->pos.y;
        } else {
            mHBController.x = con->kpad->pos.x;
            mHBController.y = con->kpad->pos.y;
        }
    }

    mHBController.trig = con->kpad->trig;
    mHBController.hold = con->kpad->hold;
    mHBController.release = con->kpad->release;

    if (con->kpad->dev_type == WPAD_DEV_CLASSIC && con->use_devtype == WPAD_DEV_CLASSIC) {
        u32 h = con->kpad->ex_status.cl.hold;
        u32 t = con->kpad->ex_status.cl.trig;
        u32 r = con->kpad->ex_status.cl.release;

        if (h & WPAD_CL_BUTTON_A) {
            mHBController.hold |= WPAD_BUTTON_A;
        }
        if (t & WPAD_CL_BUTTON_A) {
            mHBController.trig |= WPAD_BUTTON_A;
        }
        if (r & WPAD_CL_BUTTON_A) {
            mHBController.release |= WPAD_BUTTON_A;
        }

        if (h & WPAD_CL_BUTTON_PLUS) {
            mHBController.hold |= WPAD_BUTTON_PLUS;
        }
        if (t & WPAD_CL_BUTTON_PLUS) {
            mHBController.trig |= WPAD_BUTTON_PLUS;
        }
        if (r & WPAD_CL_BUTTON_PLUS) {
            mHBController.release |= WPAD_BUTTON_PLUS;
        }

        if (h & WPAD_CL_BUTTON_MINUS) {
            mHBController.hold |= WPAD_BUTTON_MINUS;
        }
        if (t & WPAD_CL_BUTTON_MINUS) {
            mHBController.trig |= WPAD_BUTTON_MINUS;
        }
        if (r & WPAD_CL_BUTTON_MINUS) {
            mHBController.release |= WPAD_BUTTON_MINUS;
        }

        if (h & WPAD_CL_BUTTON_HOME) {
            mHBController.hold |= WPAD_BUTTON_HOME;
        }
        if (t & WPAD_CL_BUTTON_HOME) {
            mHBController.trig |= WPAD_BUTTON_HOME;
        }
        if (r & WPAD_CL_BUTTON_HOME) {
            mHBController.release |= WPAD_BUTTON_HOME;
        }
    }
} // 0x803B6BE0

/*
 * clrKpadButton
 *
 * Clears the button state the menu reads.
 */
void Controller::clrKpadButton() {
    mHBController.trig = 0;
    mHBController.hold = 0;
    mHBController.release = 0;
} // 0x803B6D70

/*
 * setInValidPos
 *
 * Parks the pointer far outside the layout, so nothing is under it until WPAD
 * reports a position again.
 */
void Controller::setInValidPos() {
    mHBController.x = -10000.0f;
    mHBController.y = -15000.0f;
} // 0x803B6D90

/*
 * connect
 *
 * Connects the channel's remote speaker.
 */
void Controller::connect() {
    getRemoteSpk()->Connect(getChan());
} // 0x803B6DB0

/*
 * disconnect
 *
 * Nothing to tear down: the speaker is shared by the controllers.
 */
void Controller::disconnect() {
} // 0x803B6DC0

/*
 * setSpeakerVol
 *
 * Sets the speaker volume (the menu uses 0 to 10).
 */
void Controller::setSpeakerVol(f32 vol) {
    mHBController.spVol = vol;
} // 0x803B6DD0

/*
 * playSound
 *
 * Plays a menu sound on the remote speaker, then arms the sound-time bookkeeping
 * updateSound uses to cut a stuck sound off.
 */
void Controller::playSound(int id) {
    if (!mSoundOffFlag) {
        getRemoteSpk()->Play(getChan(), id, getSpeakerVol() * 10.0f);

        if (WPADIsSpeakerEnabled(getChan())) {
            if (!mCheckSoundTimeFlag) {
                mPlaySoundTime = OSGetTime();
            }

            mCheckSoundTimeFlag = true;
            mCheckSoundIntervalFlag = false;
        }
    }
} // 0x803B6DE0

/*
 * isPlayingSoundId
 *
 * Whether the channel's speaker is playing the given sound.
 */
bool Controller::isPlayingSoundId(int id) const {
    if (!isPlayingSound()) {
        return false;
    }

    if (!getRemoteSpk()->isPlayingId(getChan(), id)) {
        return false;
    }

    return true;
} // 0x803B6E80

/*
 * initSound
 *
 * Clears the sound-time bookkeeping.
 */
void Controller::initSound() {
    mCheckSoundTimeFlag = false;
    mCheckSoundIntervalFlag = false;
} // 0x803B6EE0

/*
 * updateSound
 *
 * Ages the sound the channel is playing and cuts it off after eight minutes;
 * when nothing is playing, waits a second after the last sound before releasing
 * the bookkeeping, and silences a speaker whose radio link has gone bad.
 */
void Controller::updateSound() {
    int chan = getChan();

    if (!isPlayingSound()) {
        if (mCheckSoundTimeFlag) {
            if (!mCheckSoundIntervalFlag) {
                mStopSoundTime = OSGetTime();
                mCheckSoundIntervalFlag = true;
            } else {
                OSTime time = OSGetTime();
                if (OSTicksToMilliseconds(OSDiffTick(time, mStopSoundTime)) >= 1000) {
                    mCheckSoundTimeFlag = false;
                    mCheckSoundIntervalFlag = false;
                }
            }
        }

        return;
    } else {
        if (mCheckSoundTimeFlag) {
            mCheckSoundIntervalFlag = false;

            OSTime time = OSGetTime();
            if (OSTicksToMilliseconds(OSDiffTick(time, mPlaySoundTime)) >= 480000) {
                mCheckSoundTimeFlag = false;
                mCheckSoundIntervalFlag = false;
                soundOff(1000);
                return;
            }
        }

        // A radio sensitivity worse than 85 means the remote is barely in range.
        if (!mSoundOffFlag && WPADGetRadioSensitivity(chan) <= 85) {
            soundOff(1000);
        }
    }
} // 0x803B6EF0

/*
 * isPlayReady
 *
 * Whether the channel's speaker is ready for another sound.
 */
bool Controller::isPlayReady() const {
    return getRemoteSpk()->isPlayReady(getChan());
} // 0x803B7130

/*
 * getController
 *
 * The menu's view of the controller state (it is the object itself).
 */
HBController* Controller::getController() {
    return &mHBController;
} // 0x803B7140

/*
 * startMotor
 *
 * Rumbles the remote unless it is playing a sound.
 */
void Controller::startMotor() {
    if (getChan() < WPAD_MAX_CONTROLLERS && mRumbleFlag && !isPlayingSound()) {
        setRumble();
        WPADControlMotor(getChan(), WPAD_MOTOR_RUMBLE);
    }
} // 0x803B7150

/*
 * stopMotor
 *
 * Stops the rumble startMotor began.
 */
void Controller::stopMotor() {
    if (getChan() < WPAD_MAX_CONTROLLERS && isRumbling()) {
        clrRumble();
        WPADControlMotor(getChan(), WPAD_MOTOR_STOP);
    }
} // 0x803B71C0

/*
 * getInfoAsync
 *
 * Asks WPAD for the remote's status; refused while the remote is busy with a
 * sound or with the motor.
 */
s32 Controller::getInfoAsync(WPADInfo* info) {
    if (getChan() >= WPAD_MAX_CONTROLLERS) {
        return -2;
    }

    if (isPlayingSound() || isRumbling()) {
        return -2;
    }

    return WPADGetInfoAsync(getChan(), info, &ControllerCallback);
} // 0x803B71F0

/*
 * ControllerCallback
 *
 * WPAD's status callback: a successful read means the battery byte in the
 * status is valid.
 */
void Controller::ControllerCallback(WPADChannel chan, WPADResult result) {
    if (result == WPAD_ESUCCESS && chan < WPAD_MAX_CONTROLLERS) {
        sBatteryFlag[chan] = true;
    }
} // 0x803B7280

/*
 * getBatteryFlag
 *
 * Whether a status read has reported the remote's battery since the flag was
 * last cleared.
 */
bool Controller::getBatteryFlag() const {
    if (getChan() >= WPAD_MAX_CONTROLLERS) {
        return false;
    }

    return sBatteryFlag[getChan()];
} // 0x803B72B0

/*
 * clrBatteryFlag
 *
 * Clears the battery flag so the menu redraws it on the next status read.
 */
void Controller::clrBatteryFlag() {
    if (getChan() >= WPAD_MAX_CONTROLLERS) {
        return;
    }

    sBatteryFlag[getChan()] = false;
} // 0x803B72E0

} // namespace homebutton
