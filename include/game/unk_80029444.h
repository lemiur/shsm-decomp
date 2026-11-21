#ifndef GAME_UNK_80029444_H
#define GAME_UNK_80029444_H

#include <rwsdk/rwtypes.h>
#include <rwsdk/plcore/bastream.h>

class CZoneChangeInfo;

/*
 * The chunk table fn_800294A4 loads: the chunk bytes, the header read at their start, the entry
 * pointers and the end of the data. Its layout is not yet reconstructed beyond what the loader
 * reads. Name invented.
 */
struct unk_ChunkTable;
struct unk_ChunkTableHeader;
struct unk_ChunkEntry;
struct unk_ChunkEntryHeader;
struct unk_ChunkRecordA;
struct unk_ChunkRecordB;
struct unk_ChunkRecordC;
struct unk_ChunkRecordD;

/* The two-word header at the start of the chunk bytes. Member names invented. */
struct unk_ChunkTableHeader {
    RwUInt16    field_0x0;
    RwUInt16    entryCount;     /* 0x2, number of unk_ChunkEntry pointers */
};

/* 0x0C-byte table: fn_800294A4 fills it, fn_8002981C frees it. Member names invented. */
struct unk_ChunkTable {
    RwUInt8*                    data;       /* 0x0, the chunk bytes, freed with delete[] */
    unk_ChunkTableHeader*       header;     /* 0x4, the same bytes */
    unk_ChunkEntry**            entries;    /* 0x8, entryCount pointers */
    RwUInt8*                    end;        /* 0xC, one past the last record */
};

/* 0x18-byte entry: a 0x40-byte header in the chunk bytes and four record arrays after it. */
struct unk_ChunkEntry {
    unk_ChunkEntryHeader*       header;     /* 0x0 */
    unk_ChunkRecordA*           recordsA;   /* 0x4, countA records */
    unk_ChunkRecordB*           recordsB;   /* 0x8, countB records */
    unk_ChunkRecordC*           recordsC;   /* 0xC, countC records */
    unk_ChunkRecordD*           recordsD;   /* 0x10, countD records */
    RwUInt32                    field_0x14; /* 0 after loading */
};

/* 0x40-byte header of an entry. The four counts index the record arrays. */
struct unk_ChunkEntryHeader {
    RwUInt16    field_0x0;
    RwUInt16    countA;         /* 0x2 */
    RwUInt16    countB;         /* 0x4 */
    RwUInt16    countC;         /* 0x6 */
    RwUInt16    countD;         /* 0x8 */
    RwUInt16    field_0xA;
    RwUInt8     field_0xC[4];   /* not byte-swapped */
    RwUInt32    field_0x10[3];
    RwUInt32    field_0x1C[3];
    RwUInt32    field_0x28[3];
    RwUInt32    field_0x34[3];
};

/* 0x14-byte record of array A. Member names invented. */
struct unk_ChunkRecordA {
    RwUInt16    field_0x0;
    RwUInt16    field_0x2;
    RwUInt16    field_0x4;
    RwUInt16    field_0x6;
    RwUInt32    field_0x8[3];
};

/* 0x0C-byte record of array B. Member names invented. */
struct unk_ChunkRecordB {
    RwUInt32    field_0x0[3];
};

/* 0x14-byte record of array C. Member names invented. */
struct unk_ChunkRecordC {
    RwUInt16    field_0x0;
    RwUInt8     field_0x2[6];   /* not byte-swapped */
    RwUInt32    field_0x8[3];
};

/* 0x18-byte record of array D. Member names invented. */
struct unk_ChunkRecordD {
    RwUInt16    field_0x0;
    RwUInt16    field_0x2;
    RwUInt8     field_0x4[4];   /* not byte-swapped */
    RwUInt32    field_0x8;
    RwUInt32    field_0xC[3];
};

extern "C" {
unk_ChunkTable* fn_800294A4( RwStream* stream, RwUInt32 chunkSize );
void            fn_8002981C( unk_ChunkTable* table );
int             fn_80029444( CZoneChangeInfo* change );
bool            fn_80090394();  /* the unit at 0x8008EEDC, not yet reconstructed */
}

#endif
