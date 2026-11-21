// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80099CC8.h>

// Bit 0 of lbl_8046BB40.field_0xA6: loop the data back to field_0xB4 at the end of the ring.
enum {
    AUDIO_DECODER_LOOP_FLAG = 0x01
};

/*
====================
AudioDecoderForOnMemory

Thread entry (OSCreateThread) that walks the on-memory audio data one block at a time, decoding
each block through AudioDecode. Blocks start with a u32 holding the stride to the next block.
When the frame index reaches the end of the ring the walk either loops back or suspends the thread.
Never returns.

NonMatching: register/schedule wall. Retail keeps the frame counter in r31 and loads the thread
address into r30 before the buffer base; the compiler assigns the three differently (wall.exe).
====================
*/
// 0x80099CC8
void* AudioDecoderForOnMemory(void* arg) {
    unk_AudioDecodeArgs args;
    u32 frame;
    u32 blockSize;

    args.data = arg;
    blockSize = lbl_8046BB40.field_0xBC;
    for (frame = 0;; frame++) {
        args.index = frame;
        AudioDecode(&args);
        if ((frame + lbl_8046BB40.field_0xC0) % lbl_8046BB40.field_0x50 == lbl_8046BB40.field_0x50 - 1) {
            if (lbl_8046BB40.field_0xA6 & AUDIO_DECODER_LOOP_FLAG) {
                blockSize = *(u32*)args.data;
                args.data = lbl_8046BB40.field_0xB4;
            } else {
                OSSuspendThread(&lbl_8046A7B0.thread);
            }
        } else {
            u32 nextSize = *(u32*)args.data;
            args.data = (u8*)args.data + blockSize;
            blockSize = nextSize;
        }
    }
}
