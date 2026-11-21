#ifndef REVOLUTION_THP___THP_H
#define REVOLUTION_THP___THP_H

#include <revolution/os.h>
#include <revolution/thp.h>

#ifdef __cplusplus
extern "C" {
#endif

/* THPPlayer.c internals: the audio decode thread and the block it runs on. The struct name is invented;
 * its layout is what CreateAudioDecodeThread touches (the thread, its stack, and two message queues). */
#define THP_AUDIO_STACK_SIZE 0x1000
#define THP_AUDIO_MSG_COUNT 6

typedef struct THPAudioDecodeBlock {
    OSThread thread;                        /* 0x000 */
    u8 stack[THP_AUDIO_STACK_SIZE];         /* 0x318 */
    OSMessageQueue queue0;                  /* 0x1318 */
    OSMessageQueue queue1;                  /* 0x1338 */
    OSMessage msgs0[THP_AUDIO_MSG_COUNT];   /* 0x1358 */
    OSMessage msgs1[THP_AUDIO_MSG_COUNT];   /* 0x1370 */
} THPAudioDecodeBlock;

extern THPAudioDecodeBlock lbl_8046A7B0;
extern BOOL lbl_80529770;

void* AudioDecoder(void* arg);
void* AudioDecoderForOnMemory(void* arg);
BOOL CreateAudioDecodeThread(OSPriority priority, void* onMemory);

/* THPDec.c internals (declared in the SDK's thp.h). */
u8 __THPReadFrameHeader(void);
u8 __THPReadScaneHeader(void);
u8 __THPReadQuantizationTable(void);
u8 __THPReadHuffmanTableSpecification(void);
void __THPPrepBitStream(void);
void __THPDecompressYUV(void* tileY, void* tileU, void* tileV);
void __THPDecompressiMCURow512x448(void);
void __THPDecompressiMCURow640x480(void);
void __THPDecompressiMCURowNxN(void);
void __THPInverseDCTNoYPos(THPCoeff* in, u32 xPos);
void __THPInverseDCTY8(THPCoeff* in, u32 xPos);
void __THPHuffDecodeDCTCompY(THPFileInfo* info, THPCoeff* block);
void __THPHuffDecodeDCTCompU(THPFileInfo* info, THPCoeff* block);
void __THPHuffDecodeDCTCompV(THPFileInfo* info, THPCoeff* block);

#ifdef __cplusplus
}
#endif

#endif
