// unit name unconfirmed: no string or class-name evidence names this file; the THP player names
// (THPPlayerQuit, THPPlayerClose) place it here. Rename it when evidence appears and keep the open
// item in docs/exceptions.md.

#include <revolution/thp/__thp.h>

THPAudioDecodeBlock lbl_8046A7B0;
BOOL lbl_80529770;

/* Starts the audio decode thread on lbl_8046A7B0. A non-NULL onMemory selects AudioDecoderForOnMemory and is its
 * argument; otherwise AudioDecoder runs with no argument. Returns FALSE when the thread cannot be created; otherwise
 * initialises the two message queues, sets lbl_80529770 and returns TRUE.
 * Not exact (85%): control flow matches; retail folds the stack top as two adds (stack base, then +0x1000) and copies
 * the priority to r8 late. Wall: other (immediate folding) and register/schedule. */
BOOL CreateAudioDecodeThread(OSPriority priority, void* onMemory)
{
    THPAudioDecodeBlock* block = &lbl_8046A7B0;

    if (onMemory != NULL) {
        if (!OSCreateThread(&block->thread, AudioDecoderForOnMemory, onMemory,
                            block->stack + THP_AUDIO_STACK_SIZE, THP_AUDIO_STACK_SIZE, priority, 1)) {
            return FALSE;
        }
    } else {
        if (!OSCreateThread(&block->thread, AudioDecoder, NULL,
                            block->stack + THP_AUDIO_STACK_SIZE, THP_AUDIO_STACK_SIZE, priority, 1)) {
            return FALSE;
        }
    }
    OSInitMessageQueue(&block->queue0, block->msgs0, THP_AUDIO_MSG_COUNT);
    OSInitMessageQueue(&block->queue1, block->msgs1, THP_AUDIO_MSG_COUNT);
    lbl_80529770 = TRUE;
    return TRUE;
} // 0x80099B78

// Resumes the audio decode thread once its flag is set. Called from THPPlayerPrepare.
// No string evidence for the name: placeholder.
void fn_80099C48(void)
{
    if (lbl_80529770) {
        OSResumeThread(&lbl_8046A7B0.thread);
    }
} // 0x80099C48
