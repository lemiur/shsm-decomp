#ifndef GAME_UNK_80016678_H
#define GAME_UNK_80016678_H

#include <revolution/nand.h>
#include <revolution/types.h>
#include <game/unk_80016634.h>

/*
 * Unit name unconfirmed (no string or class-name evidence). The NAND request dispatcher at
 * 0x80016678 shares its state with the completion callback fn_80016634 and the poll fn_80016644.
 * The class names, the request layout and the operation names are invented here; each field is
 * only as large as the code that reads it.
 */

/* Sizes the NAND layer uses here. Names invented: 0x4000 is the block the check call counts in,
 * 0x10000 the staging buffer that an open passes. */
enum {
    NAND_BLOCK_BYTES       = 0x4000,
    NAND_OPEN_BUFFER_BYTES = 0x10000
};

/* Operations a request carries in its op field. Names invented; each names the NAND call its case makes. */
enum unk_NandOp {
    NAND_OP_OPEN       = 2,
    NAND_OP_CLOSE      = 3,
    NAND_OP_READ       = 4,
    NAND_OP_CREATE     = 5,
    NAND_OP_WRITE      = 6,
    NAND_OP_SEEK       = 7,
    NAND_OP_DELETE     = 8,
    NAND_OP_CHECK      = 10,
    NAND_OP_CREATE_DIR = 11
};

/* The object that issues the NAND calls (r3 of fn_80016678). Name invented. */
struct unk_NandClient {
    u8     field_0x0[0x8];
    u32    phase;          /* 0x08: 1 or 2, set by each operation; meaning not evidenced */
    u8     field_0xC[0x28];
    void*  buffer;         /* 0x34: buffer that NANDSimpleSafeOpenAsync stages the open file through */
    u32    freeAnswer;     /* 0x38: the answer NANDCheckAsync writes */
};

/* The request the caller fills in (r4 of fn_80016678). Name invented. */
struct unk_NandRequest {
    u8     field_0x0[0x8];
    u32    op;             /* 0x08: unk_NandOp */
    char   path[0x80];     /* 0x0C: path for the calls that take one */
    void*  buffer;         /* 0x8C: data for read and write; allocated on the first read */
    s32    size;           /* 0x90: byte count */
    u32    offset;         /* 0x94: seek offset; the inode count for NAND_OP_CHECK */
};

/* Global NAND state of this dispatcher; defined in game/unk_80016678.cpp (lbl_805291D0, the state word, is owned
 * by the unit of fn_80016634 and declared in game/unk_80016634.h). */
extern NANDFileInfo     lbl_80469980;
extern NANDCommandBlock lbl_804698C0;

extern "C" {
void fn_80016678( unk_NandClient* client, unk_NandRequest* request );
}

#endif
