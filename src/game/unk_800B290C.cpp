// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_800B290C.h>

// Not reconstructed here: fn_800B2924 and fn_800B292C are compiler-generated thunks (this -= 4,
// then a tail call) of a secondary base; they belong to the class of the vtable that holds fn_800B2638 and fn_800B25BC.

u32 lbl_805273D4 = 0;

/*
====================
fn_800B290C

Returns the address of lbl_805273D4.
====================
*/
// 0x800B290C
extern "C" u32* fn_800B290C() {
    return &lbl_805273D4;
}

/*
====================
fn_800B2914

Always true.
====================
*/
// 0x800B2914
extern "C" int fn_800B2914() {
    return 1;
}

/*
====================
fn_800B291C

Always true.
====================
*/
// 0x800B291C
extern "C" int fn_800B291C() {
    return 1;
}

/*
====================
fn_800B2934

Seek that never moves the cursor: reports success.
====================
*/
// 0x800B2934
extern "C" u32 fn_800B2934( unk_800B2E58* self, u32 address ) {
    return 0;
}

/*
====================
fn_800B293C

Forwards the seek to fn_800B2ED4 with the same arguments.
====================
*/
// 0x800B293C
extern "C" u32 fn_800B293C( unk_800B2E58* self, u32 address ) {
    return fn_800B2ED4( self, address );
}

/*
====================
fn_800B2940

Moves the cursor to address. The cursor's block is (address - *bufferBase) >> 14. While that block
lies past the window's last block, the window advances one block at a time through fn_800B3144 and
the test repeats; the test is made three times, and fn_800B2FE8 finishes a seek the window cannot
reach. Returns 0 when the cursor is set or an advance fails.
Not exact: the self and address registers are swapped and the final loop has one block fewer
(wall: register/schedule and control flow; the call row shows the same shape).
====================
*/
// 0x800B2940
extern "C" u32 fn_800B2940( unk_800B2E58* self, u32 address ) {
    u32 blockOffset;
    u32 blockIndex;
    u32 blocksAhead;

    blockOffset = address - *self->bufferBase;
    blockIndex = blockOffset >> 14;
    if ( blockIndex <= self->blockCount ) {
        self->blockOffset = blockOffset;
        self->cursor = address;
        return 0;
    }
    blocksAhead = blockIndex - self->blockCount;
    while ( blocksAhead-- != 0 ) {
        if ( fn_800B3144( self, 1 ) == 0 ) {
            return 0;
        }
    }

    blockOffset = address - *self->bufferBase;
    blockIndex = blockOffset >> 14;
    if ( blockIndex <= self->blockCount ) {
        self->blockOffset = blockOffset;
        self->cursor = address;
        return 0;
    }
    blocksAhead = blockIndex - self->blockCount;
    while ( blocksAhead-- != 0 ) {
        if ( fn_800B3144( self, 1 ) == 0 ) {
            return 0;
        }
    }

    blockOffset = address - *self->bufferBase;
    blockIndex = blockOffset >> 14;
    if ( blockIndex <= self->blockCount ) {
        self->blockOffset = blockOffset;
        self->cursor = address;
        return 0;
    }
    blocksAhead = blockIndex - self->blockCount;
    while ( blocksAhead-- != 0 ) {
        if ( fn_800B3144( self, 1 ) == 0 ) {
            return 0;
        }
    }

    return fn_800B2FE8( self, address );
}
