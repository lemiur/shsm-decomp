#ifndef HBM_REMOTE_SPK_H
#define HBM_REMOTE_SPK_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/homebutton/HBMRemoteSpk.hpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMRemoteSpk.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/HBMRemoteSpk.h

#include <revolution/types.h>
#include <revolution/arc.h>
#include <revolution/os.h>
#include <revolution/wenc.h>
#include <revolution/wpad.h>

namespace homebutton {

/*
 * RemoteSpk
 *
 * Plays the Home Button Menu's sound effects on the Wii Remote speakers. The
 * sounds are PCM files in an archive; a periodic alarm encodes one packet per
 * remote and streams it. One instance at a time, reachable through
 * GetInstance() from the alarm and WPAD callbacks (0x1F8 bytes).
 */
class RemoteSpk {
private:
    /* Speaker state of one remote (0x68 bytes). */
    struct ChanInfo {
        OSAlarm alarm;     // 0x00: retries a speaker command that found WPAD busy
        WENCInfo wencinfo; // 0x30
        const s16* in_pcm; // 0x50: next sample to send; NULL when idle
        int length;        // 0x54: bytes left to send
        int seId;          // 0x58: sound playing, -1 when none
        bool first;        // 0x5C: next packet starts the stream
        s8 vol;            // 0x5D: 0 to 10
        s8 cannotSendCnt;  // 0x5E: alarm ticks WPAD refused the stream
        u16 pad;           // 0x60 pad: the name in the SDK's debug info (sdk_2009-12-11 HBMRemoteSpk.h)
        bool playReady;    // 0x62: speaker switched on and playing
    };

public:
    RemoteSpk(void* spkSeBuf);

    bool isPlayReady(WPADChannel chan) const;
    bool isPlaying(WPADChannel chan) const;
    bool isPlayingId(WPADChannel chan, int seId) const;

    void GetPCMFromSeID(int in_ID, s16*& out_wave, int& out_length);
    void ClearPcm();
    void Start();
    void Stop();
    void Connect(WPADChannel chan);
    void Play(WPADChannel chan, int seID, s8 vol);

    static void SetInstance(RemoteSpk* pThis);
    static RemoteSpk* GetInstance();

private:
    static void UpdateSpeaker(OSAlarm* alarm, OSContext* context);

    static void SpeakerOnCallback(WPADChannel chan, WPADResult result);
    static void DelaySpeakerOnCallback(OSAlarm* alarm, OSContext* context);

    static void SpeakerPlayCallback(WPADChannel chan, WPADResult result);
    static void DelaySpeakerPlayCallback(OSAlarm* alarm, OSContext* context);

    static RemoteSpk* spInstance;

    ChanInfo info[WPAD_MAX_CONTROLLERS]; // 0x000
    OSAlarm speakerAlarm;                // 0x1A0: UpdateSpeaker's period
    ARCHandle handle;                    // 0x1D0: the sound effect archive
    bool available;                      // 0x1EC: the archive opened
    // 0x1F0: vtable (declared after the members: CodeWarrior places the vptr at the first virtual)

public:
    virtual ~RemoteSpk();
};

} // namespace homebutton

#endif
