#ifndef REVOLUTION_THP_H
#define REVOLUTION_THP_H

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* THP movie decoder: JPEG-style video frames (YUV 4:2:0) and ADPCM audio frames. */

typedef u8 THPSample;
typedef s16 THPCoeff;
typedef float THPQuantTab[64];

typedef struct _THPHuffmanTab {
    u8 quick[32];
    u8 increment[32];
    u8* Vij;
    s32 maxCode[18];
    s32 valPtr[18];
    u8 Vij1;
    u8 pad[11]; // pad: SDK THPHuffmanTab.pad (rounds the table up to 224 bytes)
} THPHuffmanTab;

typedef struct _THPComponent {
    u8 quantizationTableSelector;
    u8 DCTableSelector;
    u8 ACTableSelector;
    THPCoeff predDC;
} THPComponent;

typedef struct _THPFileInfo {
    THPQuantTab quantTabs[3];
    THPHuffmanTab huffmanTabs[4];
    THPComponent components[3];
    u16 xPixelSize;
    u16 yPixelSize;
    u16 MCUsPerRow;
    u16 decompressedY;
    u8* c;
    u32 currByte;
    u32 cnt;
    u8 validHuffmanTabs;
    u8 RST;
    u16 nMCU;
    u16 currMCU;
    u8* dLC[3];
} THPFileInfo;

typedef struct {
    u32 offsetNextChannel;
    u32 sampleSize;
    s16 lCoef[8][2];
    s16 rCoef[8][2];
    s16 lYn1;
    s16 lYn2;
    s16 rYn1;
    s16 rYn2;
} THPAudioRecordHeader;

typedef struct {
    u8* encodeData;
    u32 offsetNibbles;
    u8 predictor;
    u8 scale;
    s16 yn1;
    s16 yn2;
} THPAudioDecodeInfo;

/* THPPlayer API. Members of the two info records are not recovered: they are 16-byte records the
 * game passes by address. */
typedef struct THPVideoInfo {
    u8 field_0x0[0x10];
} THPVideoInfo;

typedef struct THPAudioInfo {
    u8 field_0x0[0x10];
} THPAudioInfo;

BOOL THPPlayerPrepare(s32 frame, s32 flag, s32 audioTrack);
BOOL THPPlayerPlay(void);
BOOL THPPlayerClose(void);
void THPPlayerQuit(void);
void THPPlayerGetVideoInfo(THPVideoInfo* info);
void THPPlayerGetAudioInfo(THPAudioInfo* info);

BOOL THPInit(void);
s32 THPVideoDecode(void* file, void* tileY, void* tileU, void* tileV, void* work);
u32 THPAudioDecode(s16* audioBuffer, u8* audioFrame, s32 flag);

/* THPPlayer: the movie player the game drives. */
void THPPlayerDrawDone(void);
s32 THPPlayerGetState(void);

#ifdef __cplusplus
}
#endif

#endif
