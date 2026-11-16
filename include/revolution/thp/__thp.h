#ifndef REVOLUTION_THP___THP_H
#define REVOLUTION_THP___THP_H

#include <revolution/thp.h>

#ifdef __cplusplus
extern "C" {
#endif

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
