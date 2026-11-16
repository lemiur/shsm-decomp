// unit name unconfirmed: the unit defines CEventHandler (named by the "CEventHandler*" payload
// string) and the message registry; no string names the file.

#include <climax/EventHandler.h>
#include <string.h>

unk_801A92A8 lbl_8052A530;
unk_801A92A8 iMsgDeleteEventHandler;
unk_801A92A8 iMsgDeleteEntity;
unk_801A92A8 iMsgOnDeleteEntity;
unk_801A92A8 OnRegisterDeleteEntity;

int         lbl_8052A558;
int         lbl_8052A55C;
int         lbl_8052A560;
RwFreeList* lbl_8052A564;
int         lbl_8052A568;
int         lbl_8052A56C;
RwFreeList* lbl_8052A570;
s32         lbl_8052A574;

extern const char* const lbl_8052E408 = "iMsgDeleteEventHandler";
extern const char* const lbl_8052E40C = "iMsgDeleteEntity";
extern const char* const lbl_8052E410 = "iMsgOnDeleteEntity";
extern const char* const lbl_8052E414 = "OnRegisterDeleteEntity";

unk_EventRecord lbl_8047CC08[64];
unk_QueuedMsg   lbl_8047DA08[64];

/*
====================
HashString

Name invented.
====================
*/
static inline u32 HashString( const char* str ) {
    if ( str == NULL ) {
        return 0;
    }
    u32 hash = 0;
    for ( const char* c = str; *c != '\0'; c++ ) {
        hash = ( hash << 1 ) ^ *c;
    }
    return hash;
}

/*
====================
MatchString

True when the hashes and the strings are equal. Name invented.
====================
*/
static inline bool MatchString( u32 hashA, const char* a, u32 hashB, const char* b ) {
    if ( hashA != hashB ) {
        return false;
    }
    while ( *a != '\0' && *b != '\0' ) {
        if ( *a != *b ) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/*
====================
unk_EventRecord::IsFormat
====================
*/
inline bool unk_EventRecord::IsFormat( u32 hash, const char* str ) const {
    return MatchString( formatHash, format, hash, str );
}

/*
====================
CopyString

Sets dst to a new copy of str. Name invented.
====================
*/
static inline void CopyString( char*& dst, const char* str ) {
    dst = ( char* )operator new( strlen( str ) + 1, GAME_HEAP, GAME_HEAP_FLAGS );
    strcpy( dst, str );
}

/*
====================
HashBucket

Name invented.
====================
*/
static inline int HashBucket( const char* str ) {
    int sum = 0;
    for ( const char* c = str; *c != '\0'; c++ ) {
        sum += *c;
    }
    return sum & 63;
}

/*
====================
GetBucket

Name invented.
====================
*/
static inline unk_EventRecord& GetBucket( u32 idx ) {
    return lbl_8047CC08[idx];
}

/*
====================
GetIdFormatHash

Name invented.
====================
*/
static inline u32 GetIdFormatHash( const unk_801A92A8* id ) {
    return id->record->formatHash;
}

/*
====================
CheckFormat

Name invented.
====================
*/
static inline void CheckFormat( const unk_EventRecord* rec, u32 formatHash, const char* format ) {
    MatchString( rec->formatHash, rec->format, formatHash, format );
}

/*
====================
DispatchMsg

Sends msg to every linked handler in priority order; false when none is linked.
Name invented.
====================
*/
static inline bool DispatchMsg( unk_Msg& msg ) {
    unk_EventLink* link = msg.record->head.next;
    if ( link == NULL ) {
        return false;
    }
    msg.record->dispatching = 1;
    do {
        CEventHandler* handler = link->handler;
        if ( handler != NULL && handler->field_0x4 ) {
            handler->fn_8009CB1C( msg );
        }
        link = link->next;
    } while ( link != NULL );
    msg.record->dispatching = 0;
    if ( msg.record->purge ) {
        unk_EventLink* next;
        for ( link = msg.record->head.next; link != NULL; link = next ) {
            next = link->next;
            if ( link->handler == NULL ) {
                link->Unlink();
                delete link;
            }
        }
        msg.record->purge = 0;
    }
    return true;
}

/*
====================
unk_801A92A8::unk_801A92A8
====================
*/
// 0x801A92A8
unk_801A92A8::unk_801A92A8() {
    record = NULL;
    links = 0;
    registrations = 0;
}

/*
====================
unk_801A92A8::~unk_801A92A8
====================
*/
// 0x801A92BC
unk_801A92A8::~unk_801A92A8() {
}

/*
====================
unk_801A92A8::fn_801A92FC
====================
*/
// 0x801A92FC
void unk_801A92A8::fn_801A92FC( unk_EventRecord* record ) {
    if ( this->record != record ) {
        this->record = record;
    }
}

/*
====================
fn_801A9310

Creates the link and record free lists and registers the framework's own messages.
====================
*/
// 0x801A9310
void fn_801A9310() {
    lbl_8052A564 = RwFreeListCreateAndPreallocateSpace( sizeof( unk_EventLink ), 0x800, 16, 1, NULL, rwMEMHINTDUR_GLOBAL );
    lbl_8052A570 = RwFreeListCreateAndPreallocateSpace( sizeof( unk_EventRecord ), 0x400, 16, 1, NULL, rwMEMHINTDUR_GLOBAL );
    fn_801A93B4( iMsgDeleteEventHandler, lbl_8052E408, NULL );
    fn_801A93B4( iMsgDeleteEntity, lbl_8052E40C, NULL );
    fn_801A93B4( iMsgOnDeleteEntity, lbl_8052E410, "CEventHandler*" );
    fn_801A93B4( OnRegisterDeleteEntity, lbl_8052E414, "CAttributeHandler*" );
}

/*
====================
fn_801A93B4

Registers id as the message name with payload type format (NULL: any). A name already
registered through another id shares its record. An empty name leaves id unregistered.
====================
*/
// Not exact (register/schedule): 99.85% (338/338 rows, 8 rows differ); only the bucket-loop MatchString
// swaps record->name and name between r4 and r5 (retail: record->name in r4, name in r5; ours: r5/r4).
// 0x801A93B4
void fn_801A93B4( unk_801A92A8& id, const char* name, const char* format ) {
    if ( name == NULL || *name == '\0' ) {
        id.fn_801A92FC( lbl_8052A530.record );
        if ( id.record != lbl_8052A530.record ) {
            id.record->refCount++;
            id.registrations++;
        }
        return;
    }

    u32 nameHash = HashString( name );
    u32 formatHash = HashString( format );

    if ( id.registrations > 0 ) {
        unk_EventRecord* record = id.record;
        if ( MatchString( record->nameHash, record->name, nameHash, name ) ) {
            if ( record->format == NULL || format != NULL ) {
                if ( record->format == NULL && format != NULL ) {
                    CopyString( record->format, format );
                } else if ( record->format != NULL && MatchString( GetIdFormatHash( &id ), record->format, formatHash, format ) ) {
                    record->refCount++;
                    id.registrations++;
                    return;
                }
            }
        }
    }

    int bucket = HashBucket( name );

    for ( unk_EventRecord* record = lbl_8047CC08[bucket].next; record != NULL; record = record->next ) {
        if ( record->name == NULL || !MatchString( record->nameHash, record->name, nameHash, name ) ) {
            continue;
        }
        if ( record->format == NULL || format != NULL ) {
            if ( record->format == NULL && format != NULL ) {
                CopyString( record->format, format );
                record->formatHash = formatHash;
            } else if ( record->format != NULL ) {
                // checks the payload type; the result is unused (its report is compiled out)
                CheckFormat( record, formatHash, format );
            }
        }
        id.fn_801A92FC( record );
        id.record->refCount++;
        id.registrations++;
        return;
    }

    unk_EventRecord* record = new unk_EventRecord;
    record->next = GetBucket( bucket ).next;
    if ( record->next != NULL ) {
        record->next->prev = record;
    }
    record->prev = &lbl_8047CC08[bucket];
    lbl_8047CC08[bucket].next = record;

    CopyString( record->name, name );
    if ( format != NULL ) {
        CopyString( record->format, format );
    } else {
        record->format = NULL;
    }
    record->formatHash = formatHash;
    record->nameHash = nameHash;

    id.fn_801A92FC( record );
    id.record->refCount++;
    id.registrations++;
}

/*
====================
unk_EventRecord::unk_EventRecord
====================
*/
// 0x801A98FC
unk_EventRecord::unk_EventRecord() :
    head( NULL, 0xFFFF ), name( NULL ), format( NULL ), next( NULL ), prev( NULL ),
    refCount( 0 ), purge( 0 ), dispatching( 0 ) {
}

/*
====================
fn_801A9940

Drops one registration made through id; the record goes once nothing uses it.
====================
*/
// 0x801A9940
void fn_801A9940( unk_801A92A8& id ) {
    if ( id.record == lbl_8052A530.record ) {
        return;
    }
    unk_EventRecord* record = id.record;
    if ( id.registrations == 0 ) {
        id.fn_801A92FC( lbl_8052A530.record );
        if ( id.record != lbl_8052A530.record ) {
            id.record->refCount++;
            id.registrations++;
        }
        return;
    }

    record->refCount--;
    if ( --id.registrations == 0 ) {
        id.record = NULL;
    }
    if ( record->refCount == 0 && record->head.next == NULL ) {
        delete record->name;
        record->name = NULL;
        delete record->format;
        record->format = NULL;
        record->Unlink();

        id.fn_801A92FC( lbl_8052A530.record );
        if ( id.record != lbl_8052A530.record ) {
            id.record->refCount++;
            id.registrations++;
        }
        delete record;
    }
}

/*
====================
CEventHandler::fn_801A9AAC

Queues msg for the next fn_801A9AE8.
====================
*/
// 0x801A9AAC
void CEventHandler::fn_801A9AAC( const unk_Msg& msg ) {
    lbl_8047DA08[lbl_8052A558].used = true;
    lbl_8047DA08[lbl_8052A558].msg = msg;
    lbl_8047DA08[lbl_8052A558].sender = this;
    lbl_8052A558++;
}

/*
====================
fn_801A9AE8

Sends the queued messages.
====================
*/
// 0x801A9AE8
void fn_801A9AE8() {
    unk_QueuedMsg* queued;
    int i;
    for ( i = 0, queued = lbl_8047DA08; i < lbl_8052A558; queued++, i++ ) {
        if ( queued->used && queued->msg.record != lbl_8052A530.record && queued->msg.record->head.next != NULL ) {
            DispatchMsg( queued->msg );
        }
    }
    lbl_8052A558 = 0;
}

/*
====================
CEventHandler::~CEventHandler

Announces the handler's end with iMsgOnDeleteEntity, unlinks it from the framework's
messages and cancels the messages it queued.
====================
*/
// 0x801A9C34
CEventHandler::~CEventHandler() {
    unk_Msg msg( iMsgOnDeleteEntity, this );
    if ( msg.record != lbl_8052A530.record && msg.record->head.next != NULL ) {
        DispatchMsg( msg );
    }
    fn_801A9E00( iMsgDeleteEventHandler );
    fn_801A9E00( iMsgDeleteEntity );
    for ( int i = 0; i < lbl_8052A558; i++ ) {
        if ( lbl_8047DA08[i].used && lbl_8047DA08[i].sender == this ) {
            lbl_8047DA08[i].used = false;
        }
    }
}

/*
====================
CEventHandler::fn_801A9DE8

Links the handler to a registered message.
====================
*/
// 0x801A9DE8
void CEventHandler::fn_801A9DE8( unk_801A92A8& id, const char* format, u16 priority ) {
    if ( id.record != lbl_8052A530.record ) {
        fn_801A9F64( id, format, priority );
    }
}

/*
====================
CEventHandler::fn_801A9E00

Drops one link of the handler to the message; during a dispatch the link is only cleared.
====================
*/
// 0x801A9E00
void CEventHandler::fn_801A9E00( unk_801A92A8& id ) {
    unk_EventRecord* record = id.record;
    if ( record == lbl_8052A530.record ) {
        return;
    }
    if ( id.links == 0 ) {
        return;
    }
    if ( record->name == NULL ) {
        return;
    }
    for ( unk_EventLink* link = &record->head; link != NULL; ) {
        unk_EventLink* next = link->next;
        if ( link->handler == this ) {
            id.links--;
            if ( --link->count != 0 ) {
                return;
            }
            if ( record->dispatching ) {
                link->handler = NULL;
                record->purge = 1;
                return;
            }
            unk_EventLink* prev = link->prev;
            if ( prev != NULL ) {
                prev->next = next;
            }
            if ( next != NULL ) {
                next->prev = prev;
            }
            delete link;
            return;
        }
        link = next;
    }
}

/*
====================
DeleteHandlers

Deletes every handler linked to the record; deleting a handler unlinks it. Name invented.
====================
*/
static inline void DeleteHandlers( unk_EventRecord* record ) {
    if ( record == lbl_8052A530.record ) {
        return;
    }
    for ( unk_EventLink* link = record->head.next; link != NULL; link = record->head.next ) {
        delete link->handler;
    }
}

/*
====================
fn_801A9ED8

Deletes every handler linked to the message.
====================
*/
// 0x801A9ED8
void fn_801A9ED8( unk_801A92A8& id ) {
    unk_EventRecord* record = id.record;
    if ( record != lbl_8052A530.record ) {
        DeleteHandlers( record );
    }
}

/*
====================
CEventHandler::fn_801A9F44

Stops delivery to the handler and links it to iMsgDeleteEntity.
====================
*/
// 0x801A9F44
void CEventHandler::fn_801A9F44() {
    field_0x4 = 0;
    fn_801A9F64( iMsgDeleteEntity, NULL, 0x8000 );
}

/*
====================
CEventHandler::fn_801A9F64

Links the handler to the message at priority; false when format is not the message's
payload type or no link is free. A repeated link only counts.
====================
*/
// 0x801A9F64
bool CEventHandler::fn_801A9F64( unk_801A92A8& id, const char* format, u16 priority ) {
    unk_EventRecord* record = id.record;
    if ( record != lbl_8052A530.record ) {
        id.links++;
        for ( unk_EventLink* link = record->head.next; link != NULL; link = link->next ) {
            if ( link->handler == this ) {
                link->count++;
                return true;
            }
        }
        if ( format != NULL && record->format != NULL ) {
            if ( !record->IsFormat( HashString( format ), format ) ) {
                return false;
            }
        }

        unk_EventLink* link = new unk_EventLink( this, priority );
        if ( link == NULL ) {
            return false;
        }
        unk_EventLink* prev = &id.record->head;
        unk_EventLink* next;
        for ( next = prev->next; next != NULL; next = next->next ) {
            if ( prev->priority >= priority && next->priority <= priority ) {
                prev->next = link;
                link->prev = prev;
                next->prev = link;
                link->next = next;
                return true;
            }
            prev = next;
        }
        prev->next = link;
        link->prev = prev;
    }
    return true;
}

/*
====================
CEventHandler::fn_801AA17C

True when the handler is linked to the message.
====================
*/
// 0x801AA17C
bool CEventHandler::fn_801AA17C( unk_801A92A8& id ) {
    if ( id.record == lbl_8052A530.record ) {
        return false;
    }
    for ( unk_EventLink* link = id.record->head.next; link != NULL; link = link->next ) {
        if ( link->handler == this ) {
            return true;
        }
    }
    return false;
}

/*
====================
fn_801AA1C4

Registers id again under name.
====================
*/
// 0x801AA1C4
void fn_801AA1C4( unk_801A92A8& id, const char* name, const char* format ) {
    fn_801A9940( id );
    fn_801A93B4( id, name, format );
}

/*
====================
CEventHandler::fn_801AA218

Registers id again under name and links the handler to it.
====================
*/
// 0x801AA218
void CEventHandler::fn_801AA218( unk_801A92A8& id, const char* name, const char* format ) {
    fn_801A9E00( id );
    fn_801A9940( id );
    fn_801A93B4( id, name, format );
    fn_801A9DE8( id, format );
}

/*
====================
fn_801AA2AC

Sends msg to every linked handler in priority order; false when none is linked.
====================
*/
// 0x801AA2AC
bool fn_801AA2AC( unk_Msg& msg ) {
    return DispatchMsg( msg );
}

/*
====================
fn_801AA3C8

Plugin constructor: no handler yet.
====================
*/
// 0x801AA3C8
static void* fn_801AA3C8( void* object, s32 offset, s32 size ) {
    *PLUGIN_OFFSET( unk_EventHandlerPlugin, object, lbl_8052A574 ) = unk_EventHandlerPlugin();
    return object;
}

/*
====================
fn_801AA3D8

Plugin copy.
====================
*/
// 0x801AA3D8
static void* fn_801AA3D8( void* dst, const void* src, s32 offset, s32 size ) {
    *PLUGIN_OFFSET( unk_EventHandlerPlugin, dst, lbl_8052A574 ) =
        *PLUGIN_OFFSET( const unk_EventHandlerPlugin, src, lbl_8052A574 );
    return dst;
}

/*
====================
fn_801AA3E8

Registers a pointer-sized plugin (id 0x7FF); false on failure.
====================
*/
// 0x801AA3E8
bool fn_801AA3E8() {
    lbl_8052A574 = RpAtomicRegisterPlugin( sizeof( unk_EventHandlerPlugin ), 0x7FF, fn_801AA3C8, NULL, fn_801AA3D8 );
    return lbl_8052A574 != 0;
}
