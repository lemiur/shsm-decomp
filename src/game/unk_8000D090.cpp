// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000D090.h>
#include <game/unk_PhysicsManager.h>
#include <climax/Heap.h>

unk_PhysicsManager* lbl_80529170;

/*
====================
unk_8000D090::unk_8000D090
====================
*/
// Parked (wall.exe): control flow isomorphic; register/schedule and frame layout differ from retail.
// 0x8000D090
unk_8000D090::unk_8000D090( const unk_RwpShapeDef* shapeDef, const unk_RwpBodyDef& bodyDef, void* arg6, void* arg7,
                            const float* sizes, const unk_RwV3d* position ) {
    unk_SpaceBox box;
    unk_RwpSpace* newSpace;
    unk_PhysicsSpaceLink* link;
    unk_PhysicsManager* manager;
    unk_RwpWorld* world;
    int i;

    /* The space's box is the shape's box shrunk by half a unit on every side. */
    box.min.x = shapeDef->lo.x + 0.5f;
    box.min.y = shapeDef->lo.y + 0.5f;
    box.min.z = shapeDef->lo.z + 0.5f;
    box.max.x = shapeDef->hi.x - 0.5f;
    box.max.y = shapeDef->hi.y - 0.5f;
    box.max.z = shapeDef->hi.z - 0.5f;

    fn_80325594( &params, shapeDef );
    fn_80325634( &params, 0x80 );
    fn_8032B9C8( &params, 10, 10 );
    fn_802FAC88( &params, 0x28, 0x28 );
    fn_803255F8( &params, 0 );
    fn_80325624( &params, 0x40 );
    fn_8032562C( &params, 0x40 );
    newSpace = fn_8031E078( &params, &box, 1.0f );
    space = newSpace;

    /* Register the space with the manager, at the end of its list. */
    link = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_PhysicsSpaceLink( newSpace );
    manager = lbl_80529170;
    link->prev = manager->lastSpace;
    if ( manager->lastSpace != NULL ) {
        manager->lastSpace->next = link;
    }
    if ( manager->spaces == NULL ) {
        manager->spaces = link;
    }
    manager->lastSpace = link;

    bits = RwpBitTableCreate( 10, 10, manager->spaceCount++ );

    /* Bits of the space's table: the corners and edges of its index grid. */
    unk_RwpBitTableSet( bits, bits->count * 4 + 4 );
    unk_RwpBitTableSet( bits, bits->count * 4 + 5 );
    unk_RwpBitTableSet( bits, bits->count * 5 + 4 );
    unk_RwpBitTableSet( bits, bits->count * 3 + 5 );
    unk_RwpBitTableSet( bits, bits->count * 5 + 3 );
    for ( i = 2; i < 10; i += 2 ) {
        unk_RwpBitTableSet( bits, bits->count * 6 + i );
        unk_RwpBitTableSet( bits, i * bits->count + 6 );
        unk_RwpBitTableSet( bits, bits->count * 6 + i + 1 );
        unk_RwpBitTableSet( bits, ( i + 1 ) * bits->count + 6 );
    }
    for ( i = 1; i < 10; i++ ) {
        unk_RwpBitTableSet( bits, bits->count * 7 + i );
        unk_RwpBitTableSet( bits, i * bits->count + 7 );
    }
    for ( i = 2; i < 10; i++ ) {
        unk_RwpBitTableSet( bits, bits->count * 8 + i );
        unk_RwpBitTableSet( bits, i * bits->count + 8 );
        unk_RwpBitTableSet( bits, bits->count * 9 + i );
        unk_RwpBitTableSet( bits, i * bits->count + 9 );
    }
    unk_RwpBitTableClear( bits, bits->count * 8 + 8 );
    unk_RwpBitTableClear( bits, bits->count * 9 + 9 );

    world = fn_8031E364( space );
    fn_80320D48( world, bits );

    body = bodyDef;
    field_0xF8 = arg6;
    field_0xFC = arg7;
    this->shapeDef = shapeDef;
    shape = fn_8030E904( shapeDef );
    fn_8030F770( shape, 1 );
    fn_8030F768( shape, 0.01f );
    fn_80310778( shape, sizes[ 1 ] );
    fn_80310880( shape, sizes[ 0 ] );

    fn_8031E288( space, shape, 0 );
    fn_8032207C( fn_8031E35C( space ), position );
    this->position = *position;
}
