#ifndef GAME_UNK_DECODETHREADCONTEXT_H
#define GAME_UNK_DECODETHREADCONTEXT_H

#include <revolution/os.h>
#include <revolution/types.h>

/*
 * The decode thread block at lbl_8046D4D0: the thread that runs VideoDecoder or
 * VideoDecoderForOnMemory, the stack it starts on (the thread starts at the stack's end), and
 * two message queues with their message buffers. Name invented.
 * symbols.txt sizes lbl_8046D4D0 as 0x1318; the message queues lie past that size (open).
 */
struct unk_DecodeThreadContext {
    OSThread        thread;                 // 0x000
    u8              stack[0x1000];          // 0x318
    OSMessageQueue  messageQueue[2];        // 0x1318
    OSMessage       messageBuffer[2][3];    // 0x1358
};

extern unk_DecodeThreadContext lbl_8046D4D0;   // defined elsewhere, unit not yet reconstructed
extern BOOL lbl_805297A0;                       // set when CreateVideoDecodeThread starts the thread; read by fn_8009C308
extern BOOL lbl_805297A4;                       // set together with lbl_805297A0

#endif
