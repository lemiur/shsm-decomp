#ifndef GAME_UNK_800ABE6C_H
#define GAME_UNK_800ABE6C_H

#include <revolution/types.h>
#include <climax/EventHandler.h>
#include <climax/Heap.h>
#include <climax/Time.h>
#include <game/unk_800B0C64.h>

/*
 * A byte block with a 16-bit length, read from a stream by fn_800ABFE4 and fn_800AC078.
 * Name invented; the element type is unknown (the block is allocated as u8[count]).
 */
struct unk_ByteBlock {
    u8*     data;       // 0x0, NULL until a read allocates it
    u16     count;      // 0x4, the length read from the stream
};


/* Globals used by fn_800ABE6C; owned by units not yet reconstructed. */
extern void*    lbl_80529940;       // the 0x104-byte object fn_800ABE6C creates once
extern u8       lbl_80529878;
extern s32      lbl_80527348;
extern u8       lbl_8046E958[0x80];
extern u8       lbl_8046E9D8[0x80];

extern "C" {
int     fn_800ABE6C();
u32     fn_800ABFE0();
int     fn_800ABFE4( unk_ByteBlock* block, void* stream );
int     fn_800AC078( unk_ByteBlock* block, void* stream );

/* Constructor of the object fn_800ABE6C creates (its class is not yet named). */
void    fn_800B6210( void* self );

/* Stream readers of other units; signatures taken from their call sites here. */
void    fn_800AF064( void* stream, u8* dst, u16 count );
void    fn_800AF088( void* stream, void* dst, int size );
void    fn_800AF090( void* stream, u8* dst, u16 count );
void    fn_800AF188( void* stream, void* dst, int size );
}

#endif
