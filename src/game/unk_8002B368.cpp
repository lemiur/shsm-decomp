// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8002B368.h>

/*
====================
fn_8002B6AC
====================
*/
// 0x8002B6AC
extern "C" unk_EdgeRef* fn_8002B6AC( unk_EdgeRef* self, unk_80133818* p ) {
    unk_EdgeRef hold( p );
    if ( p ) {
        p->refCount++;
    }
    unk_80133818* old = self->m_p;
    self->m_p = p;
    if ( old ) {
        if ( --old->refCount == 0 ) {
            delete old;
        }
    }
    return self;
}

/*
====================
fn_8002B77C
====================
*/
// 0x8002B77C
extern "C" unk_80133818* fn_8002B77C( unk_80133818* self, s32 flags ) {
    if ( self ) {
        fn_801336A0( self, 0 );
        if ( flags > 0 ) {
            operator delete( self );
        }
    }
    return self;
}

/*
====================
fn_8002B368
====================
*/
// Retail passes 0x40000 as the allocation flags of every edge and cell (name not recovered).
// 0x8002B368
extern "C" void fn_8002B368( void* owner, unk_EdgeNodeRange* range ) {
    unk_EdgeNode** end = range->end;
    for ( unk_EdgeNode** i = range->begin; i != end; i++ ) {
        for ( unk_EdgeNode** j = i; j != end; j++ ) {
            if ( i == j ) {
                continue;
            }
            unk_EdgeNode* a = *j;
            unk_EdgeNode* b = *i;

            unk_EdgeRef first( new ( 2, 0x40000 ) unk_803F5F10( b, a ) );
            unk_EdgeCell* cellB = new ( 2, 0x40000 ) unk_EdgeCell( first.m_p );
            cellB->next = b->edgeHead;
            cellB->prev = 0;
            if ( b->edgeHead ) {
                b->edgeHead->prev = cellB;
            }
            if ( !b->edgeTail ) {
                b->edgeTail = cellB;
            }
            b->edgeHead = cellB;
            b->edgeCount++;

            unk_EdgeRef second( new ( 2, 0x40000 ) unk_803F5F10( a, b ) );
            unk_EdgeCell* cellA = new ( 2, 0x40000 ) unk_EdgeCell( second.m_p );
            cellA->next = a->edgeHead;
            cellA->prev = 0;
            if ( a->edgeHead ) {
                a->edgeHead->prev = cellA;
            }
            if ( !a->edgeTail ) {
                a->edgeTail = cellA;
            }
            a->edgeHead = cellA;
            a->edgeCount++;
        }
    }
}
