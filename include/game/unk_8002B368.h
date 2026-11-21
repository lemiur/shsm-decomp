#ifndef GAME_UNK_8002B368_H
#define GAME_UNK_8002B368_H

// unit name unconfirmed: header name invented; the classes below are placeholders (names invented).

#include <revolution/types.h>

void operator delete( void* p );
void* operator new( unsigned long size, int heapKind, int align );

struct unk_EdgeCell;

// Graph node that keeps a list of the edges touching it. Role name invented; only the
// edge-list members the factory touches are known.
struct unk_EdgeNode {
    u8              field_0x0[0x14];
    s32             edgeCount;      // 0x14
    unk_EdgeCell*   edgeHead;       // 0x18
    unk_EdgeCell*   edgeTail;       // 0x1C
};

// Node range passed by the caller: begin and end of an array of node pointers.
struct unk_EdgeNodeRange {
    unk_EdgeNode**  begin;          // 0x0
    unk_EdgeNode**  end;            // 0x4
};

// Reference-counted edge base, 0x14 bytes. Constructor and vtable slots 0xC..0x14 are out of line.
class unk_80133818 {
public:
    unk_80133818( unk_EdgeNode* a, unk_EdgeNode* b );
    virtual ~unk_80133818();        // 0x8002B77C
    virtual void fn_80133870();
    virtual void fn_80133928();
    virtual void fn_80133810();

    s32             refCount;       // 0x4
    u32             field_0x8;      // 0x8
    u32             field_0xC;      // 0xC
    f32             field_0x10;     // 0x10
};

// Derived edge (0x14 bytes). Vtable lbl_803F5F10.
class unk_803F5F10 : public unk_80133818 {
public:
    unk_803F5F10( unk_EdgeNode* a, unk_EdgeNode* b ) : unk_80133818( a, b ) {}
    virtual void fn_80133870();     // overrides slot 0xC (fn_80123EA0)
};

// One list cell (0xC bytes): retained edge link plus its neighbours in the node's list.
struct unk_EdgeCell {
    unk_EdgeCell( unk_80133818* l ) : link( l ), next( 0 ), prev( 0 ) {
        if ( l ) {
            l->refCount++;
        }
    }
    unk_80133818*   link;           // 0x0: retained
    unk_EdgeCell*   next;           // 0x4
    unk_EdgeCell*   prev;           // 0x8
};

// Smart reference to an edge: retains on copy, releases (deleting the edge at zero) on destruction.
class unk_EdgeRef {
public:
    unk_80133818*   m_p;
    unk_EdgeRef() : m_p( 0 ) {}
    unk_EdgeRef( unk_80133818* p ) : m_p( p ) {
        if ( m_p ) {
            m_p->refCount++;
        }
    }
    ~unk_EdgeRef() {
        if ( m_p ) {
            if ( --m_p->refCount == 0 ) {
                delete m_p;
            }
        }
    }
};

extern "C" {
void fn_8002B368( void* owner, unk_EdgeNodeRange* range );
unk_EdgeRef* fn_8002B6AC( unk_EdgeRef* self, unk_80133818* p );
unk_80133818* fn_8002B77C( unk_80133818* self, s32 flags );
void fn_801336A0( void* self, s32 flags );
}

#endif
