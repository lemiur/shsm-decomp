#ifndef GAME_UNK_80099CC8_H
#define GAME_UNK_80099CC8_H

#include <revolution/types.h>
#include <revolution/os.h>
#include <revolution/thp/__thp.h>

// Argument block that AudioDecoderForOnMemory passes to AudioDecode (names invented).
// The decoder thread fills index with its frame counter before each call.
struct unk_AudioDecodeArgs {
    void* data;   // current position in the on-memory audio data
    u32 index;    // frame counter of the decoder thread
    u8 field_0x8[0x18 - 0x8];   // the rest of the block, used by AudioDecode only
};

// The movie player state (lbl_8046BB40, 0x200 bytes; names invented). Only the fields that
// AudioDecoderForOnMemory and the movie-frame draw read are named; the rest is unknown.
struct unk_MovieFrame;
struct unk_AudioDecoderState {
    u8 field_0x0[0x50];
    u32 field_0x50;         // ring length in frames: the slot index wraps modulo this
    u8 field_0x54[0x80 - 0x54];
    s32 field_0x80;
    s32 field_0x84;
    u8 field_0x88[0xA0 - 0x88];
    s32 field_0xA0;
    u8 field_0xA4;
    u8 field_0xA5;
    u8 field_0xA6;          // flags: bit 0 loops the data back to field_0xB4 at the ring end
    u8 field_0xA7[0xB4 - 0xA7];
    void* field_0xB4;       // start of the data, the loop-back position
    u8 field_0xB8[0xBC - 0xB8];
    u32 field_0xBC;         // size of the first block (initial block stride)
    u32 field_0xC0;         // offset added to the frame counter before the ring modulus
    u8 field_0xC4[0xF8 - 0xC4];
    unk_MovieFrame* frame;  // 0xF8, NULL until the first frame is decoded
    u8 field_0xFC[0x200 - 0xFC];
};

// Shared with the other audio decoder functions (lbl_8046A7B0 and AudioDecode are declared in revolution/thp/__thp.h).
// Their owning unit is not reconstructed yet.
extern unk_AudioDecoderState lbl_8046BB40;

extern "C" {
void* AudioDecoderForOnMemory(void* arg);
}

#endif
