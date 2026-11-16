#ifndef CLIMAX_EVENTHANDLER_H
#define CLIMAX_EVENTHANDLER_H

#include <climax/Memory.h>
#include <climax/unk_801A92A8.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/bamemory.h>

class CEventHandler;

/* RenderWare plugin callbacks (RwPluginObjectConstructor/Copy in the RenderWare 3.7 SDK). */
typedef void* ( *unk_PluginConstructor )( void* object, s32 offset, s32 size );
typedef void* ( *unk_PluginCopy )( void* dst, const void* src, s32 offset, s32 size );

extern "C" {

// defined in 0x802D1A58 (RenderWare), unit not yet reconstructed: registers a plugin of size
// bytes with an atomic and returns its offset in the atomic (C linkage, as RenderWare is C)
s32 RpAtomicRegisterPlugin( s32 size, u32 pluginID, unk_PluginConstructor constructor,
                            unk_PluginConstructor destructor, unk_PluginCopy copy );

}

/* The plugin data of type, offset bytes into object (RWPLUGINOFFSET in the RenderWare SDK). */
#define PLUGIN_OFFSET( type, object, offset ) \
    ( ( type* )( ( u8* )( object ) + ( offset ) ) )

/* The plugin data fn_801AA3E8 registers: the handler owning the object. Name invented. */
struct unk_EventHandlerPlugin {
    unk_EventHandlerPlugin() : handler( NULL ) {}

    CEventHandler*  handler;    // 0x0
};

extern int          lbl_8052A558;   // queued messages
extern int          lbl_8052A55C;   // links in use
extern int          lbl_8052A560;   // peak links, in steps of 32
extern RwFreeList*  lbl_8052A564;   // link free list
extern int          lbl_8052A568;   // records in use
extern int          lbl_8052A56C;   // peak records, in steps of 64
extern RwFreeList*  lbl_8052A570;   // record free list
extern s32          lbl_8052A574;   // offset of the plugin data

/*
 * One handler linked to a message: a node of the message's list, kept in descending
 * priority order behind the list head in unk_EventRecord. Links come from a free list
 * (lbl_8052A564). Name invented.
 */
struct unk_EventLink {
            unk_EventLink( CEventHandler* handler, u16 priority ) :
                next( NULL ), prev( NULL ), handler( handler ), count( 1 ), priority( priority ) {}

    static void*    operator new( size_t size ) {
                        lbl_8052A55C++;
                        if ( lbl_8052A55C > lbl_8052A560 ) {
                            lbl_8052A560 += 32;
                        }
                        return RwFreeListAlloc( lbl_8052A564, rwMEMHINTDUR_GLOBAL );
                    }
    static void     operator delete( void* block ) {
                        if ( block != NULL ) {
                            lbl_8052A55C--;
                            RwFreeListFree( lbl_8052A564, block );
                        }
                    }

    void            Unlink() {
                        unk_EventLink* before = prev;
                        unk_EventLink* after = next;
                        if ( before != NULL ) {
                            before->next = after;
                        }
                        if ( after != NULL ) {
                            after->prev = before;
                        }
                    }

    unk_EventLink*  next;       // 0x00
    unk_EventLink*  prev;       // 0x04
    CEventHandler*  handler;    // 0x08, NULL once unlinked during a dispatch
    int             count;      // 0x0C, times the handler linked
    u16             priority;   // 0x10, higher first
};

/*
 * A registered message: its handler list, name and payload type with their hashes, and
 * its place in the hash bucket lbl_8047CC08[]. Records come from a free list
 * (lbl_8052A570). Name invented.
 */
struct unk_EventRecord {
                    unk_EventRecord();

    static void*    operator new( size_t size ) {
                        lbl_8052A568++;
                        if ( lbl_8052A568 > lbl_8052A56C ) {
                            lbl_8052A56C += 64;
                        }
                        return RwFreeListAlloc( lbl_8052A570, rwMEMHINTDUR_GLOBAL );
                    }
    static void     operator delete( void* block ) {
                        if ( block != NULL ) {
                            lbl_8052A568--;
                            RwFreeListFree( lbl_8052A570, block );
                        }
                    }

    /* True when the payload type and its hash equal str and hash; defined in EventHandler.cpp. */
    inline bool     IsFormat( u32 hash, const char* str ) const;

    void            Unlink() {
                        unk_EventRecord* before = prev;
                        unk_EventRecord* after = next;
                        if ( before != NULL ) {
                            before->next = after;
                        }
                        if ( after != NULL ) {
                            after->prev = before;
                        }
                    }

    unk_EventLink       head;           // 0x00, list head; its priority is the highest
    char*               name;           // 0x14
    char*               format;         // 0x18, payload type such as "CEventHandler*"; may be NULL
    u32                 nameHash;       // 0x1C
    u32                 formatHash;     // 0x20
    unk_EventRecord*    next;           // 0x24, bucket list
    unk_EventRecord*    prev;           // 0x28
    int                 refCount;       // 0x2C, registrations of the name
    int                 purge;          // 0x30, links were cleared during a dispatch
    int                 dispatching;    // 0x34
};

/* A message: the registered message and its payload. Name invented. */
struct unk_Msg {
            unk_Msg() {}
            unk_Msg( const unk_801A92A8& id, void* data ) : record( id.record ), data( data ) {}

    unk_EventRecord*    record;     // 0x0
    void*               data;       // 0x4
};

/* A message posted for the next fn_801A9AE8, and the handler that posted it. Name invented. */
struct unk_QueuedMsg {
    bool            used;       // 0x0
    unk_Msg         msg;        // 0x4
    CEventHandler*  sender;     // 0xC
};

/*
 * The base of every object that receives messages (named by the payload type string
 * "CEventHandler*" of iMsgOnDeleteEntity, whose payload is the handler being destroyed).
 */
class CEventHandler {
public:
                    CEventHandler() : field_0x4( 1 ) {}
    virtual         ~CEventHandler();
    virtual void    fn_8009CB1C( unk_Msg& msg ) = 0;

    void            fn_801A9AAC( const unk_Msg& msg );
    void            fn_801A9DE8( unk_801A92A8& id, const char* format = NULL, u16 priority = 0x8000 );
    void            fn_801A9E00( unk_801A92A8& id );
    void            fn_801A9F44();
    bool            fn_801A9F64( unk_801A92A8& id, const char* format, u16 priority );
    bool            fn_801AA17C( unk_801A92A8& id );
    void            fn_801AA218( unk_801A92A8& id, const char* name, const char* format );

    int             field_0x4;  // nonzero: messages are delivered
};

extern unk_801A92A8 lbl_8052A530;      // never registered: the id of no message
extern unk_801A92A8 iMsgDeleteEventHandler;
extern unk_801A92A8 iMsgDeleteEntity;
extern unk_801A92A8 iMsgOnDeleteEntity;
extern unk_801A92A8 OnRegisterDeleteEntity;

extern const char* const lbl_8052E408;  // "iMsgDeleteEventHandler"
extern const char* const lbl_8052E40C;  // "iMsgDeleteEntity"
extern const char* const lbl_8052E410;  // "iMsgOnDeleteEntity"
extern const char* const lbl_8052E414;  // "OnRegisterDeleteEntity"

extern unk_EventRecord  lbl_8047CC08[64];
extern unk_QueuedMsg    lbl_8047DA08[64];

void    fn_801A9310();
void    fn_801A93B4( unk_801A92A8& id, const char* name, const char* format );
void    fn_801A9940( unk_801A92A8& id );
void    fn_801A9AE8();
void    fn_801A9ED8( unk_801A92A8& id );
void    fn_801AA1C4( unk_801A92A8& id, const char* name, const char* format );
bool    fn_801AA2AC( unk_Msg& msg );

/* Sends id with data when a handler is linked to it. Name invented. */
inline void PostMsg( unk_801A92A8& id, void* data ) {
    if ( id.record != lbl_8052A530.record && id.record->head.next != NULL ) {
        unk_Msg msg( id, data );
        fn_801AA2AC( msg );
    }
}

/* The registered name of id; NULL when it has no record. Name invented. */
inline const char* GetMsgName( const unk_801A92A8& id ) {
    if ( id.record == NULL ) {
        return NULL;
    }
    return id.record->name;
}

/*
 * Whether id names a registered message. Name invented. The test is bound to a named result:
 * where it is inlined, MWCC then keeps it as a value (subf, subf, or, srwi.), as CDrawingTest's
 * retail does, where PostMsg's plain test compiles to a compare and branch.
 */
inline bool IsRegistered( const unk_801A92A8& id ) {
    bool registered = id.record != lbl_8052A530.record;
    return registered;
}

/* PostMsg through IsRegistered, returning fn_801AA2AC's result: the return inside the test leaves
   retail's second branch where the expansion ends a then-branch (CDrawingTest::fn_801796F8). Name
   invented. */
inline bool PostRegisteredMsg( unk_801A92A8& id, void* data ) {
    unk_EventRecord* record = id.record;
    bool registered = record != lbl_8052A530.record;
    if ( registered && record->head.next != NULL ) {
        unk_Msg msg;
        msg.record = record;
        msg.data = data;
        return fn_801AA2AC( msg );
    }
    return false;
}
bool    fn_801AA3E8();

#endif
