// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/HBMAxSound.cpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMAxSound.cpp
#include "HBMAxSound.h"

#include <math.h>

#include <revolution/ai.h>
#include <revolution/arc.h>
#include <revolution/ax.h>
#include <revolution/hbm.h>
#include <revolution/os.h>

#include "sound/mix.h"
#include "sound/seq.h"
#include "sound/syn.h"

namespace {

/* Messages the audio thread receives. */
enum AudioThreadMsg {
    AudioThreadMsg_RunAudioFrame = 1 << 0, // sent by the AX frame callback
    AudioThreadMsg_Exit = 1 << 3           // sent by ShutdownAxSound
};

/* Sequence players per list (names invented). */
enum {
    DEFAULT_PLAYER_NUM = 4,
    FOCUS_PLAYER_NUM = 3
};

/* The audio thread's minimum stack, the last part of the HBM_MEM_SIZE_SOUND work memory
 * (names invented). The stack takes everything after the work area, so a larger memSize
 * gives a larger stack. */
#define SOUND_THREAD_STACK_SIZE 0x4000
#define SOUND_WORK_AREA_SIZE    (HBM_MEM_SIZE_SOUND - SOUND_THREAD_STACK_SIZE)

typedef int UserCallback(int evt, int num);

/* A sequence player and its link in the list of playing sequences (0x2E2C bytes). */
struct SeqPlayer {
    HBMSEQSEQUENCE seq; // 0x0000
    bool activeFlag;    // 0x2E1C: taken by a sequence
    SeqPlayer* next;    // 0x2E20
    SeqPlayer* prev;    // 0x2E24
    int seqNum;         // 0x2E28: index into SOUND_FILENAME
};

/* The playing sequences in start order, oldest first. */
struct SeqPlayerList {
    SeqPlayer* head; // 0x0
    SeqPlayer* tail; // 0x4
};

/* The sound work area at the start of the memory given to InitAxSound (0x146BC bytes). */
struct HBMAxSoundWork {
    SeqPlayer playersDefault[DEFAULT_PLAYER_NUM]; // 0x00000
    SeqPlayer playersFocus[FOCUS_PLAYER_NUM];     // 0x0B8B0: cursor sounds
    SeqPlayerList playerListDefault;              // 0x14334
    SeqPlayerList playerListFocus;                // 0x1433C
    UserCallback* userCallback;                   // 0x14344
    AXOutCallback oldAxRegisterCallback;          // 0x14348: the callback AudioFrameCallback replaced
    ARCHandle soundArcHandle;                     // 0x1434C
    OSThread mThread;                             // 0x14368: runs AudioSoundThreadProc
    OSMessageQueue mMsgQueue;                     // 0x14680
    OSMessage mMessageArray[4];                   // 0x146A0
    u64* mpThreadStack;                           // 0x146B0: SOUND_WORK_AREA_SIZE into the memory
    void* wt;                                     // 0x146B4: wavetable file in the archive
    void* pcm;                                    // 0x146B8: sample file in the archive
};

void* GetMidiDataFromArc(int num);
SeqPlayerList* GetUsePlayerListFromSeqNum(int num);
SeqPlayer* GetFreePlayer(int num);
void AudioFrameCallback();
void* AudioSoundThreadProc(void* arg);
void StopSeq(SeqPlayer* player);
void UpdateSeqPlayerList(SeqPlayerList* list);

static const char WT_FILENAME[] = "wt\\HomeButtonSe.wt";
static const char PCM_FILENAME[] = "wt\\HomeButtonSe.pcm";
static HBMAxSoundWork* sWork;

/*
 * GetMidiDataFromArc
 *
 * Returns the MIDI file of sound num in the sound archive, or NULL if it is missing.
 */
void* GetMidiDataFromArc(int num) {
    static const char* SOUND_FILENAME[] = {
        "midi\\00_home_button.mid",
        "midi\\01_return_app.mid",
        "midi\\02_goto_menu.mid",
        "midi\\03_reset_app.mid",
        "midi\\04_focus.mid",
        "midi\\05_select.mid",
        "midi\\06_cancel.mid",
        "midi\\07_open_controller.mid",
        "midi\\08_close_controller.mid",
        "midi\\09_volume_plus.mid",
        "midi\\10_volume_minus.mid",
        "midi\\11_volume_plus_limit.mid",
        "midi\\12_volume_minus_limit.mid",
        "midi\\13_nothing_done.mid",
        "midi\\14_vibe_on.mid",
        "midi\\15_vibe_off.mid",
        "midi\\16_start_connect_window.mid",
        "midi\\17_connected1.mid",
        "midi\\18_connected2.mid",
        "midi\\19_connected3.mid",
        "midi\\20_connected4.mid",
        "midi\\21_end_connect_window.mid",
        "midi\\22_manual_open.mid",
        "midi\\23_manual_focus.mid",
        "midi\\24_manual_select.mid",
        "midi\\25_manual_scroll.mid",
        "midi\\26_manual_cancel.mid",
        "midi\\27_manual_return_app.mid"
    };

    ARCFileInfo info;
    BOOL result = ARCOpen(&sWork->soundArcHandle, SOUND_FILENAME[num], &info);
    if (!result) {
        return NULL;
    }

    return ARCGetStartAddrInMem(&info);
} // no copy in retail (inlined)

/*
 * GetUsePlayerListFromSeqNum
 *
 * The focus sounds play in their own list; every other sound in the default list.
 */
SeqPlayerList* GetUsePlayerListFromSeqNum(int num) {
    switch (num) {
    case 4:  // "04_focus.mid"
    case 23: // "23_manual_focus.mid"
    case 25: // "25_manual_scroll.mid"
        return &sWork->playerListFocus;

    default:
        return &sWork->playerListDefault;
    }
} // no copy in retail (inlined)

/*
 * GetFreePlayer
 *
 * Takes a free player of the list sound num plays in; when all are busy, stops the oldest
 * sequence of that list and takes its player.
 */
SeqPlayer* GetFreePlayer(int num) {
    SeqPlayer* playersArray;
    int arraySize;
    switch (num) {
    case 4:  // "04_focus.mid"
    case 23: // "23_manual_focus.mid"
    case 25: // "25_manual_scroll.mid"
        playersArray = sWork->playersFocus;
        arraySize = FOCUS_PLAYER_NUM;
        break;

    default:
        playersArray = sWork->playersDefault;
        arraySize = DEFAULT_PLAYER_NUM;
        break;
    }

    SeqPlayer* player = NULL;
    for (int i = 0; i < arraySize; i++) {
        if (!playersArray[i].activeFlag) {
            player = &playersArray[i];
            player->activeFlag = true;
            break;
        }
    }

    if (!player) {
        SeqPlayerList* list = GetUsePlayerListFromSeqNum(num);

        player = list->head;
        StopSeq(list->head);
        player->activeFlag = true;
    }

    return player;
} // 0x803B7C20

/*
 * AudioFrameCallback
 *
 * AX frame callback: wakes the audio thread, then chains to the callback it replaced.
 */
void AudioFrameCallback() {
    if (!sWork) {
        return;
    }

    OSSendMessage(&sWork->mMsgQueue, reinterpret_cast<OSMessage>(AudioThreadMsg_RunAudioFrame),
                  OS_MESSAGE_NOBLOCK);

    if (sWork->oldAxRegisterCallback) {
        (*sWork->oldAxRegisterCallback)();
    }
} // 0x803B7DC0

/*
 * AudioSoundThreadProc
 *
 * The audio thread: runs the sequencer, synthesizer and mixer once per audio frame and
 * releases the players whose sequences have finished, until told to exit.
 */
void* AudioSoundThreadProc(void*) {
    OSMessage message = 0;

    while (true) {
        OSReceiveMessage(&sWork->mMsgQueue, &message, OS_MESSAGE_BLOCK);

        if (reinterpret_cast<u32>(message) == AudioThreadMsg_RunAudioFrame) {
            HBMSEQRunAudioFrame();
            HBMSYNRunAudioFrame();
            HBMMIXUpdateSettings();

            if (sWork) {
                UpdateSeqPlayerList(&sWork->playerListDefault);
                UpdateSeqPlayerList(&sWork->playerListFocus);
            }
        } else if (reinterpret_cast<u32>(message) == AudioThreadMsg_Exit) {
            break;
        }
    }

    return NULL;
} // 0x803B7E30

/*
 * StopSeq
 *
 * Stops the player's sequence, frees the player and unlinks it from its list.
 */
void StopSeq(SeqPlayer* player) {
    SeqPlayerList* list = GetUsePlayerListFromSeqNum(player->seqNum);

    HBMSEQSetState(&player->seq, HBMSEQ_STATE_STOP);
    HBMSEQRemoveSequence(&player->seq);

    player->activeFlag = false;

    if (!player->prev) {
        list->head = player->next;
    } else {
        player->prev->next = player->next;
    }

    if (!player->next) {
        list->tail = player->prev;
    } else {
        player->next->prev = player->prev;
    }

    player->next = NULL;
    player->prev = NULL;
} // no copy in retail (inlined)

/*
 * UpdateSeqPlayerList
 *
 * Releases the players of the list whose sequences have stopped and whose notes have all
 * ended.
 */
void UpdateSeqPlayerList(SeqPlayerList* list) {
    SeqPlayer* current = list->head;
    for (SeqPlayer* next; current; current = next) {
        next = current->next;

        if (current->activeFlag && HBMSEQGetState(&current->seq) == HBMSEQ_STATE_STOP &&
            !current->seq.synth.notes)
        {
            StopSeq(current);
        }
    }
} // no copy in retail (inlined)

/*
 * GainToTenthsDb
 *
 * Converts a linear gain to tenths of a decibel.
 */
s32 GainToTenthsDb(f32 gain) {
    return static_cast<s32>(static_cast<f32>(log10(gain)) * 20.0f * 10.0f);
} // no copy in retail (inlined)

} // namespace

namespace homebutton {

/*
 * PlaySeq
 *
 * Starts sound num on a free player and appends the player to its list.
 */
void PlaySeq(int num) {
    void* midi = GetMidiDataFromArc(num);
    if (!midi) {
        return;
    }

    SeqPlayer* player = GetFreePlayer(num);
    HBMSEQAddSequence(&player->seq, static_cast<u8*>(midi), static_cast<u8*>(sWork->wt),
                      static_cast<u8*>(sWork->pcm), NULL);
    HBMSEQSetState(&player->seq, HBMSEQ_STATE_RUN);
    player->seqNum = num;

    SeqPlayerList* list = GetUsePlayerListFromSeqNum(num);
    if (!list->tail) {
        list->head = player;
        list->tail = player;

        player->next = NULL;
        player->prev = NULL;
    } else {
        list->tail->next = player;

        player->prev = list->tail;
        player->next = NULL;

        list->tail = player;
    }
} // 0x803B8070

/*
 * InitAxSound
 *
 * Starts the menu's sound in mem (at least HBM_MEM_SIZE_SOUND bytes): initialises AX if the
 * application has not, opens the wavetable and samples in the sound archive soundData, and
 * starts the audio thread on the rest of mem.
 */
void InitAxSound(const void* soundData, void* mem, u32 memSize) {
    if (memSize < HBM_MEM_SIZE_SOUND) {
        return;
    }

    if (!AICheckInit()) {
        AIInit(NULL);
        AXInit();
    }

    HBMMIXInit();
    HBMSYNInit();
    HBMSEQInit();

    HBMAxSoundWork* work = static_cast<HBMAxSoundWork*>(mem);

    for (int i = 0; i < DEFAULT_PLAYER_NUM; i++) {
        work->playersDefault[i].activeFlag = false;
        work->playersDefault[i].next = NULL;
        work->playersDefault[i].prev = NULL;
    }

    for (int i = 0; i < FOCUS_PLAYER_NUM; i++) {
        work->playersFocus[i].activeFlag = false;
        work->playersFocus[i].next = NULL;
        work->playersFocus[i].prev = NULL;
    }

    work->wt = NULL;
    work->pcm = NULL;
    work->mpThreadStack = reinterpret_cast<u64*>(static_cast<u8*>(mem) + SOUND_WORK_AREA_SIZE);
    work->playerListDefault.head = NULL;
    work->playerListDefault.tail = NULL;
    work->playerListFocus.head = NULL;
    work->playerListFocus.tail = NULL;
    work->oldAxRegisterCallback = NULL;

    BOOL result = ARCInitHandle(const_cast<void*>(soundData), &work->soundArcHandle);
    if (!result) {
        return;
    }

    ARCFileInfo wtInfo;
    result = ARCOpen(&work->soundArcHandle, WT_FILENAME, &wtInfo);
    if (!result) {
        return;
    }

    work->wt = ARCGetStartAddrInMem(&wtInfo);

    ARCFileInfo pcmInfo;
    result = ARCOpen(&work->soundArcHandle, PCM_FILENAME, &pcmInfo);
    if (!result) {
        return;
    }

    work->pcm = ARCGetStartAddrInMem(&pcmInfo);

    OSInitMessageQueue(&work->mMsgQueue, work->mMessageArray, 4);

    // The stack grows down from the end of mem.
    result = OSCreateThread(&work->mThread, AudioSoundThreadProc, NULL,
                            reinterpret_cast<u8*>(work->mpThreadStack) + memSize - SOUND_WORK_AREA_SIZE,
                            memSize - SOUND_WORK_AREA_SIZE, 4, 0);
    if (!result) {
        return;
    }

    sWork = work;
    OSResumeThread(&work->mThread);

    BOOL intrStatus = OSDisableInterrupts();

    work->oldAxRegisterCallback = AXRegisterCallback(&AudioFrameCallback);

    OSRestoreInterrupts(intrStatus);
} // 0x803B81B0

/*
 * ShutdownAxSound
 *
 * Stops every sequence, restores the AX callback, ends the audio thread and shuts the sound
 * libraries down.
 */
void ShutdownAxSound() {
    if (!sWork) {
        return;
    }

    StopAllSeq();
    AXRegisterCallback(sWork->oldAxRegisterCallback);

    OSJamMessage(&sWork->mMsgQueue, reinterpret_cast<OSMessage>(AudioThreadMsg_Exit), OS_MESSAGE_BLOCK);

    OSJoinThread(&sWork->mThread, NULL);
    HBMSEQQuit();
    HBMSYNQuit();
    HBMMIXQuit();
    sWork = NULL;
} // 0x803B83B0

/*
 * AxSoundMain
 *
 * Nothing to do: the audio thread does the per-frame work.
 */
void AxSoundMain() {} // 0x803B8440

/*
 * StopAllSeq
 */
void StopAllSeq() {
    if (!sWork) {
        return;
    }

    for (int i = 0; i < DEFAULT_PLAYER_NUM; i++) {
        SeqPlayer& player = sWork->playersDefault[i];

        if (player.activeFlag) {
            StopSeq(&player);
        }
    }

    for (int i = 0; i < FOCUS_PLAYER_NUM; i++) {
        SeqPlayer& player = sWork->playersFocus[i];

        if (player.activeFlag) {
            StopSeq(&player);
        }
    }
} // 0x803B8450

/*
 * SetVolumeAllSeq
 *
 * Sets the volume of every playing sequence to the linear gain, clamped to -90.4..+6 dB.
 */
void SetVolumeAllSeq(f32 gain) {
    if (!sWork) {
        return;
    }

    s32 db;
    if (gain <= 0.0f) {
        db = -904;
    } else {
        db = GainToTenthsDb(gain);
    }

    if (db > 60) {
        db = 60;
    }

    if (db < -904) {
        db = -904;
    }

    for (int i = 0; i < DEFAULT_PLAYER_NUM; i++) {
        SeqPlayer& player = sWork->playersDefault[i];

        if (player.activeFlag) {
            HBMSEQSetVolume(&player.seq, db);
        }
    }

    for (int i = 0; i < FOCUS_PLAYER_NUM; i++) {
        SeqPlayer& player = sWork->playersFocus[i];

        if (player.activeFlag) {
            HBMSEQSetVolume(&player.seq, db);
        }
    }
} // 0x803B8620

/*
 * SetSoundMode
 */
void SetSoundMode(u32 mode) {
    HBMMIXSetSoundMode(mode);
} // 0x803B8740

} // namespace homebutton
