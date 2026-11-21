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

// Audio decoder state (lbl_8046BB40, names invented). Only the fields that
// AudioDecoderForOnMemory reads are named; the rest of the 0x200-byte object is unknown.
struct unk_AudioDecoderState {
    u8 field_0x0[0x50];
    u32 field_0x50;         // ring length in frames: the slot index wraps modulo this
    u8 field_0x54[0xA6 - 0x54];
    u8 field_0xA6;          // flags: bit 0 loops the data back to field_0xB4 at the ring end
    u8 field_0xA7[0xB4 - 0xA7];
    void* field_0xB4;       // start of the data, the loop-back position
    u8 field_0xB8[0xBC - 0xB8];
    u32 field_0xBC;         // size of the first block (initial block stride)
    u32 field_0xC0;         // offset added to the frame counter before the ring modulus
    u8 field_0xC4[0x200 - 0xC4];
};

// Shared with the other audio decoder functions (lbl_8046A7B0 is declared in revolution/thp/__thp.h).
// Their owning unit is not reconstructed yet.
extern unk_AudioDecoderState lbl_8046BB40;

extern "C" {
void AudioDecode(unk_AudioDecodeArgs* args);
void* AudioDecoderForOnMemory(void* arg);
}

#endif
