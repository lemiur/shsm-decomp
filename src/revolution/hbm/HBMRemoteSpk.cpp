// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/HBMRemoteSpk.cpp (VERSION_R4QK01 branches)
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMRemoteSpk.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/HBMRemoteSpk.cpp
#include "HBMRemoteSpk.h"

#include <revolution/arc.h>
#include <revolution/os.h>
#include <revolution/wenc.h>
#include <revolution/wpad.h>

namespace homebutton {

/* Speaker stream timing, as nw4hbm::snd::RemoteSpeaker streams it (names invented). */
enum {
    SAMPLES_PER_PACKET = 40,     // PCM samples encoded per packet
    ENCODED_PACKET_SIZE = 20,    // bytes of ADPCM a packet encodes to
    MAX_CANNOT_SEND_COUNT = 60,  // alarm ticks without a send before a sound is dropped
    RETRY_DELAY_MSEC = 50        // wait before retrying a speaker command WPAD found busy
};

#define SPEAKER_ALARM_PERIOD_NSEC 6666667 // one packet every 1/150 s (name invented)

/* The full volume of RemoteSpk::Play (name invented). */
#define MAX_VOLUME 10

RemoteSpk* RemoteSpk::spInstance;

static bool MakeVolumeData(const s16* src, s16* dst, int vol, u32 size);

/*
 * SetInstance
 *
 * Records the RemoteSpk the alarm and WPAD callbacks work on.
 */
void RemoteSpk::SetInstance(RemoteSpk* pThis) {
    spInstance = pThis;
} // no copy in retail (inlined)

/*
 * GetInstance
 *
 * The current RemoteSpk, or NULL when none exists.
 */
RemoteSpk* RemoteSpk::GetInstance() {
    return spInstance;
} // no copy in retail (inlined)

/*
 * GetPCMFromSeID
 *
 * Finds sound in_ID in the sound effect archive: its samples and their size in bytes.
 */
void RemoteSpk::GetPCMFromSeID(int in_ID, s16*& out_wave, int& out_length) {
    ARCFileInfo af;
    ARCFastOpen(&handle, in_ID, &af);

    out_wave = static_cast<s16*>(ARCGetStartAddrInMem(&af));
    out_length = ARCGetLength(&af);

    ARCClose(&af);
} // no copy in retail (inlined)

/*
 * MakeVolumeData
 *
 * Copies one packet of samples from src to dst scaled to vol, padding a short last
 * packet with silence. size is the number of samples left in the sound; returns false
 * while more packets follow.
 */
static bool MakeVolumeData(const s16* src, s16* dst, int vol, u32 size) {
    u32 enc_size = size <= SAMPLES_PER_PACKET ? size : SAMPLES_PER_PACKET;
    for (int i = 0; i < enc_size; i++) {
        *dst++ = static_cast<s16>(*src++ * vol / MAX_VOLUME);
    }

    if (size > SAMPLES_PER_PACKET) {
        return false;
    }

    u32 zero_size = SAMPLES_PER_PACKET - size;
    for (int i = 0; i < zero_size; i++) {
        *dst++ = 0;
    }

    return true;
} // no copy in retail (inlined)

/*
 * UpdateSpeaker
 *
 * speakerAlarm handler: sends the next packet of each playing sound to its remote.
 * A sound whose remote refuses the stream for too long is dropped.
 */
void RemoteSpk::UpdateSpeaker(OSAlarm*, OSContext*) {
    s16 pcmBuffer[SAMPLES_PER_PACKET];
    u8 adpcmBuffer[ENCODED_PACKET_SIZE];

    if (!GetInstance()) {
        return;
    }

    ChanInfo* pinfo = GetInstance()->info;
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++, pinfo++) {
        if (pinfo->in_pcm && WPADIsSpeakerEnabled(i)) {
            BOOL intrStatus = OSDisableInterrupts();

            if (WPADCanSendStreamData(i)) {
                MakeVolumeData(pinfo->in_pcm, pcmBuffer, pinfo->vol, pinfo->length / sizeof(s16));
                WENCGetEncodeData(&pinfo->wencinfo, pinfo->first ? 0 : WENC_FLAG_USER_INFO, pcmBuffer,
                                  SAMPLES_PER_PACKET, adpcmBuffer);
                WPADSendStreamData(i, adpcmBuffer, ENCODED_PACKET_SIZE);

                pinfo->first = false;
                pinfo->cannotSendCnt = 0;
                pinfo->in_pcm += SAMPLES_PER_PACKET;
                pinfo->length -= sizeof pcmBuffer;

                if (pinfo->length <= 0) {
                    pinfo->seId = -1;
                    pinfo->in_pcm = NULL;
                }
            } else {
                pinfo->cannotSendCnt++;

                if (pinfo->cannotSendCnt > MAX_CANNOT_SEND_COUNT) {
                    pinfo->in_pcm = NULL;
                }
            }

            OSRestoreInterrupts(intrStatus);
        }
    }
} // 0x803B7300

/*
 * ClearPcm
 *
 * Stops the sound on every remote.
 */
void RemoteSpk::ClearPcm() {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        info[i].in_pcm = NULL;
        info[i].seId = -1;
    }
} // 0x803B7570

/*
 * RemoteSpk
 *
 * Opens the sound effect archive at spkSeBuf (none: every call does nothing) and
 * becomes the current instance.
 */
RemoteSpk::RemoteSpk(void* spkSeBuf) {
    SetInstance(this);

    if (spkSeBuf) {
        available = ARCInitHandle(spkSeBuf, &handle);
    } else {
        available = false;
    }

    OSCreateAlarm(&speakerAlarm);
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCreateAlarm(&info[i].alarm);
        info[i].in_pcm = NULL;
        info[i].seId = -1;
        info[i].first = true;
        info[i].playReady = true;
    }
} // 0x803B75A0

/*
 * ~RemoteSpk
 *
 * Cancels every alarm and clears the current instance.
 */
RemoteSpk::~RemoteSpk() {
    SetInstance(NULL);

    available = false;

    OSCancelAlarm(&speakerAlarm);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCancelAlarm(&info[i].alarm);
    }
} // 0x803B7660

/*
 * Start
 *
 * Starts the streaming alarm and resets every remote's state.
 */
void RemoteSpk::Start() {
    if (!available) {
        return;
    }

    OSCreateAlarm(&speakerAlarm);
    OSSetPeriodicAlarm(&speakerAlarm, OSGetTime(), OSNanosecondsToTicks(SPEAKER_ALARM_PERIOD_NSEC),
                       &UpdateSpeaker);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCreateAlarm(&info[i].alarm);
        info[i].in_pcm = NULL;
        info[i].seId = -1;
        info[i].first = true;
        info[i].playReady = true;
    }
} // 0x803B7710

/*
 * Stop
 *
 * Stops the streaming alarm and every pending retry.
 */
void RemoteSpk::Stop() {
    OSCancelAlarm(&speakerAlarm);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCancelAlarm(&info[i].alarm);
    }
} // 0x803B77F0

/*
 * DelaySpeakerOnCallback
 *
 * Retry alarm: switches the speaker of the remote in the alarm's user data on again.
 */
void RemoteSpk::DelaySpeakerOnCallback(OSAlarm* alarm, OSContext*) {
    WPADChannel chan = reinterpret_cast<WPADChannel>(OSGetAlarmUserData(alarm));
    WPADControlSpeaker(chan, WPAD_SPEAKER_ON, &SpeakerOnCallback);
} // 0x803B7850

/*
 * SpeakerOnCallback
 *
 * The speaker switched on: start playback. WPAD busy: retry after a delay.
 */
void RemoteSpk::SpeakerOnCallback(WPADChannel chan, WPADResult result) {
    RemoteSpk* pRmtSpk = GetInstance();
    if (!pRmtSpk) {
        return;
    }

    switch (result) {
    case WPAD_ERR_OK:
        pRmtSpk->info[chan].first = true;
        WPADControlSpeaker(chan, WPAD_SPEAKER_PLAY, &SpeakerPlayCallback);
        break;

    case WPAD_ERR_TRANSFER:
        pRmtSpk->info[chan].playReady = false;
        break;

    case WPAD_ERR_BUSY:
        OSSetAlarmUserData(&pRmtSpk->info[chan].alarm, reinterpret_cast<void*>(chan));
        OSCancelAlarm(&pRmtSpk->info[chan].alarm);
        OSSetAlarm(&pRmtSpk->info[chan].alarm, OSMillisecondsToTicks(RETRY_DELAY_MSEC), &DelaySpeakerOnCallback);
        break;
    }
} // 0x803B7880

/*
 * DelaySpeakerPlayCallback
 *
 * Retry alarm: starts playback on the remote in the alarm's user data again.
 */
void RemoteSpk::DelaySpeakerPlayCallback(OSAlarm* alarm, OSContext*) {
    WPADChannel chan = reinterpret_cast<WPADChannel>(OSGetAlarmUserData(alarm));
    WPADControlSpeaker(chan, WPAD_SPEAKER_PLAY, &SpeakerPlayCallback);
} // 0x803B7960

/*
 * SpeakerPlayCallback
 *
 * Playback started: the remote is ready. Failed or gone: not ready. WPAD busy: retry
 * after a delay.
 */
void RemoteSpk::SpeakerPlayCallback(WPADChannel chan, WPADResult result) {
    RemoteSpk* pRmtSpk = GetInstance();
    if (!pRmtSpk) {
        return;
    }

    switch (result) {
    case WPAD_ERR_OK:
        pRmtSpk->info[chan].playReady = true;
        break;

    case WPAD_ERR_TRANSFER:
        pRmtSpk->info[chan].playReady = false;
        break;

    case WPAD_ERR_NO_CONTROLLER:
        pRmtSpk->info[chan].playReady = false;
        break;

    case WPAD_ERR_BUSY:
        OSSetAlarmUserData(&pRmtSpk->info[chan].alarm, reinterpret_cast<void*>(chan));
        OSCancelAlarm(&pRmtSpk->info[chan].alarm);
        OSSetAlarm(&pRmtSpk->info[chan].alarm, OSMillisecondsToTicks(RETRY_DELAY_MSEC), &DelaySpeakerPlayCallback);
        break;
    }
} // 0x803B7990

/*
 * Connect
 *
 * Switches on the speaker of a newly connected remote and resets its encoder; the
 * remote is ready once SpeakerPlayCallback says so.
 */
void RemoteSpk::Connect(WPADChannel chan) {
    if (!available) {
        return;
    }

    WPADControlSpeaker(chan, WPAD_SPEAKER_ON, &SpeakerOnCallback);

    u32* p = reinterpret_cast<u32*>(info[chan].wencinfo.data);
    for (int i = sizeof info[chan].wencinfo.data / sizeof(u32); i > 0; i--, p++) {
        *p = 0;
    }

    info[chan].first = true;
    info[chan].playReady = false;
} // 0x803B7A80

/*
 * Play
 *
 * Starts sound seID on a remote at volume vol (0 to 10), replacing the sound playing.
 */
void RemoteSpk::Play(WPADChannel chan, int seID, s8 vol) {
    if (!available) {
        return;
    }

    s16* pcm;
    int length;
    GetPCMFromSeID(seID, pcm, length);

    info[chan].cannotSendCnt = 0;
    info[chan].seId = seID;
    info[chan].length = length;
    info[chan].vol = vol;
    info[chan].in_pcm = pcm;
} // 0x803B7B10

/*
 * isPlaying
 *
 * Whether a sound is playing on the remote.
 */
bool RemoteSpk::isPlaying(WPADChannel chan) const {
    return info[chan].in_pcm != NULL;
} // 0x803B7BB0

/*
 * isPlayingId
 *
 * Whether sound seId is playing on the remote.
 */
bool RemoteSpk::isPlayingId(WPADChannel chan, int seId) const {
    if (isPlaying(chan) && info[chan].seId == seId) {
        return true;
    } else {
        return false;
    }
} // 0x803B7BD0

/*
 * isPlayReady
 *
 * Whether the remote's speaker is on and playing.
 */
bool RemoteSpk::isPlayReady(WPADChannel chan) const {
    return info[chan].playReady != false;
} // 0x803B7C00

} // namespace homebutton
