// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80029444.h>
#include <game/CZoneChangeInfo.h>
#include <game/CGenericSpawner.h>
#include <game/unk_ResourceManager.h>
#include <climax/Heap.h>
#include <rwsdk/plcore/babinary.h>

/*
====================
fn_80029444

Predicate on a zone change: false unless the change's flag at 0x97 is set. When the flag at 0x95
is set as well, the result is whether the resource manager's flag at 0x10 is clear. It is false
while fn_80090394 holds. Flag names invented.

Exact: the result is int because the flag at 0x97 is read as its byte value (a bool would add a
normalising test that retail does not have).
====================
*/
// 0x80029444
extern "C" int fn_80029444( CZoneChangeInfo* change ) {
    int result = change->field_0x97;
    if ( result && change->field_0x95 != 0 ) {
        result = lbl_8052A5C0->field_0x10 == 0;
    }
    if ( fn_80090394() ) {
        result = false;
    }
    return result;
}

/*
====================
fn_800294A4

Loads a chunk table from the stream. The chunk holds chunkSize - 12 bytes of data (a 12-byte chunk
header precedes them) whose multi-byte fields are little-endian, so they are swapped in place. The
table header and every entry are swapped, and each entry points at its header and its four record
arrays inside the copy. Returns NULL when the table cannot be allocated.

Open: the match is 98.65%; the wall is register/schedule only (the callee-saved numbering of the
table, the counters and the record offsets). Control flow is isomorphic.
====================
*/
// 0x800294A4
extern "C" unk_ChunkTable* fn_800294A4( RwStream* stream, RwUInt32 chunkSize ) {
    unk_ChunkTable* table = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ChunkTable;
    if ( table != NULL ) {
        table->data = NULL;
        table->header = NULL;
        table->entries = NULL;
        table->end = NULL;

        table->data = new ( GAME_HEAP, GAME_HEAP_FLAGS ) RwUInt8[ chunkSize - 12 ];
        RwStreamRead( stream, table->data, chunkSize - 12 );

        table->header = (unk_ChunkTableHeader*)table->data;
        RwMemNative16( &table->header->field_0x0, sizeof( table->header->field_0x0 ) );
        RwMemNative16( &table->header->entryCount, sizeof( table->header->entryCount ) );
        table->entries = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ChunkEntry*[ table->header->entryCount ];

        RwUInt8* cursor = table->data + sizeof( unk_ChunkTableHeader );
        int i;
        for ( i = 0; i < table->header->entryCount; i++ ) {
            unk_ChunkEntry* entry = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_ChunkEntry;
            if ( entry != NULL ) {
                entry->field_0x14 = 0;
                entry->header = (unk_ChunkEntryHeader*)cursor;
                cursor += sizeof( unk_ChunkEntryHeader );
                RwMemNative16( &entry->header->field_0x0, sizeof( entry->header->field_0x0 ) );
                RwMemNative16( &entry->header->countA, sizeof( entry->header->countA ) );
                RwMemNative16( &entry->header->countB, sizeof( entry->header->countB ) );
                RwMemNative16( &entry->header->countC, sizeof( entry->header->countC ) );
                RwMemNative16( &entry->header->countD, sizeof( entry->header->countD ) );
                RwMemNative16( &entry->header->field_0xA, sizeof( entry->header->field_0xA ) );

                entry->recordsA = (unk_ChunkRecordA*)cursor;
                cursor += entry->header->countA * sizeof( unk_ChunkRecordA );
                entry->recordsB = (unk_ChunkRecordB*)cursor;
                cursor += entry->header->countB * sizeof( unk_ChunkRecordB );
                entry->recordsC = (unk_ChunkRecordC*)cursor;
                cursor += entry->header->countC * sizeof( unk_ChunkRecordC );
                entry->recordsD = (unk_ChunkRecordD*)cursor;
                cursor += entry->header->countD * sizeof( unk_ChunkRecordD );

                RwMemNative32( entry->header->field_0x10, sizeof( entry->header->field_0x10 ) );
                RwMemNative32( entry->header->field_0x1C, sizeof( entry->header->field_0x1C ) );
                RwMemNative32( entry->header->field_0x28, sizeof( entry->header->field_0x28 ) );
                RwMemNative32( entry->header->field_0x34, sizeof( entry->header->field_0x34 ) );

                int j;
                for ( j = 0; j < entry->header->countA; j++ ) {
                    RwMemNative16( &entry->recordsA[j].field_0x0, sizeof( entry->recordsA[j].field_0x0 ) );
                    RwMemNative16( &entry->recordsA[j].field_0x2, sizeof( entry->recordsA[j].field_0x2 ) );
                    RwMemNative16( &entry->recordsA[j].field_0x4, sizeof( entry->recordsA[j].field_0x4 ) );
                    RwMemNative16( &entry->recordsA[j].field_0x6, sizeof( entry->recordsA[j].field_0x6 ) );
                    RwMemNative32( entry->recordsA[j].field_0x8, sizeof( entry->recordsA[j].field_0x8 ) );
                }
                for ( j = 0; j < entry->header->countB; j++ ) {
                    RwMemNative32( entry->recordsB[j].field_0x0, sizeof( entry->recordsB[j].field_0x0 ) );
                }
                for ( j = 0; j < entry->header->countC; j++ ) {
                    RwMemNative16( &entry->recordsC[j].field_0x0, sizeof( entry->recordsC[j].field_0x0 ) );
                    RwMemNative32( entry->recordsC[j].field_0x8, sizeof( entry->recordsC[j].field_0x8 ) );
                }
                for ( j = 0; j < entry->header->countD; j++ ) {
                    RwMemNative16( &entry->recordsD[j].field_0x0, sizeof( entry->recordsD[j].field_0x0 ) );
                    RwMemNative16( &entry->recordsD[j].field_0x2, sizeof( entry->recordsD[j].field_0x2 ) );
                    RwMemNative32( &entry->recordsD[j].field_0x8, sizeof( entry->recordsD[j].field_0x8 ) );
                    RwMemNative32( entry->recordsD[j].field_0xC, sizeof( entry->recordsD[j].field_0xC ) );
                }
            }
            table->entries[i] = entry;
        }
        table->end = cursor;
    }
    return table;
}

/*
====================
fn_8002981C

Frees a chunk table: each entry, the entry pointers, the chunk bytes and the table. The record
arrays live inside the chunk bytes and are freed with them.

Open: the match is 98.89%; the wall is register/schedule only (the two loop counters swap their
callee-saved registers). Control flow is isomorphic.
====================
*/
// 0x8002981C
extern "C" void fn_8002981C( unk_ChunkTable* table ) {
    if ( table != NULL ) {
        int i;
        for ( i = 0; i < table->header->entryCount; ++i ) {
            delete table->entries[i];
        }
        delete[] table->entries;
        delete[] table->data;
        table->end = NULL;
        delete table;
    }
}
