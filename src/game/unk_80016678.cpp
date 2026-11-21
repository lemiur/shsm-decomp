// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80016678.h>
#include <climax/Heap.h>

// Keeps buffer and its size as the pending pair (the previous buffer is deleted). Name invented.
extern "C" void fn_80014AF4( void* buffer, s32 size );
// Handles the operations this dispatcher does not start itself. Name invented.
extern "C" void fn_80014314( unk_NandClient* client, unk_NandRequest* request );

NANDFileInfo     lbl_80469980;
NANDCommandBlock lbl_804698C0;

/*
====================
fn_80016678
====================
*/
/*
 * Starts the NAND call for one request and marks the NAND state busy (lbl_805291D0 = 1); the
 * callback marks it done. Retail reads neither output of NANDGetFreeSpace: the case only passes
 * its locals. The read buffer's byte count is retail's (size + 0x400) * 2 - (size + 0x400).
 * Wall: register/schedule, three rows in NAND_OP_CHECK (the offset load, the block-count addition
 * and addze keep retail's slots only in part).
 */
// 0x80016678
void fn_80016678( unk_NandClient* client, unk_NandRequest* request ) {
    switch ( request->op ) {
    case NAND_OP_OPEN:
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDSimpleSafeOpenAsync( request->path, &lbl_80469980, NAND_ACCESS_RW, client->buffer,
                                 NAND_OPEN_BUFFER_BYTES,
                                 fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_CLOSE:
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDSimpleSafeCloseAsync( &lbl_80469980, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_READ:
        if ( request->buffer == NULL ) {
            request->buffer = new ( GAME_HEAP, GAME_HEAP_FLAGS ) u8[ ( request->size + 0x400 ) * 2 - ( request->size + 0x400 ) ];
            fn_80014AF4( request->buffer, request->size );
        }
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDReadAsync( &lbl_80469980, request->buffer, request->size, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_CREATE:
        client->phase = 2;
        lbl_805291D0 = 1;
        NANDCreateAsync( request->path, 0x30, 0, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_WRITE:
        client->phase = 2;
        lbl_805291D0 = 1;
        NANDWriteAsync( &lbl_80469980, request->buffer, request->size, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_SEEK:
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDSeekAsync( &lbl_80469980, request->offset, NAND_SEEK_BEG, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_DELETE:
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDDeleteAsync( request->path, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_CREATE_DIR:
        client->phase = 2;
        lbl_805291D0 = 1;
        NANDCreateDirAsync( request->path, 0x30, 0, fn_80016634, &lbl_804698C0 );
        break;

    case NAND_OP_CHECK: {
        u32 freeBlocks;
        u32 freeInodes;

        NANDGetFreeSpace( &freeBlocks, &freeInodes );
        s32 blocks;

        client->freeAnswer = 0;
        blocks = ( request->size + ( NAND_BLOCK_BYTES - 1 ) ) / NAND_BLOCK_BYTES;
        client->phase = 1;
        lbl_805291D0 = 1;
        NANDCheckAsync( blocks, request->offset, &client->freeAnswer, fn_80016634, &lbl_804698C0 );
        break;
    }

    default:
        fn_80014314( client, request );
        break;
    }
}
