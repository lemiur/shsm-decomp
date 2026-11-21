#ifndef GAME_UNK_8009C234_H
#define GAME_UNK_8009C234_H

#include <game/unk_DecodeThreadContext.h>
#include <revolution/os.h>
#include <revolution/types.h>

extern "C" {
/* The two decode thread entries, defined in units not yet reconstructed (0x8009C360, 0x8009C42C). */
void* VideoDecoder( void* param );
void* VideoDecoderForOnMemory( void* param );

/* Starts the video decode thread at priority; a non-NULL param selects VideoDecoderForOnMemory. */
BOOL CreateVideoDecodeThread( OSPriority priority, void* param );
}

#endif
