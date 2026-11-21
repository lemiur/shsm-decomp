// CEnemySpawn: class name from the registration string lbl_803E34E4 ("CEnemySpawn"), registered by the
// static initialiser at 0x80052E58.
// Unit name and boundary: string literal "CEnemySpawn" and the registration in the static initialiser.

#include <game/CEnemySpawn.h>

/*
====================
CEnemySpawn::fn_8005305C

The type-registry factory: builds a CEnemySpawn that keeps the packet it was created from.
Not matched: one extra `mr r4, r30` (the packet copied into r4) precedes the CBaseSpawner
constructor call in retail; wall.exe finds no class for it (control flow isomorphic).
====================
*/
// 0x8005305C
CAttributeHandler* CEnemySpawn::fn_8005305C( const unk_AttributePacket& packet ) {
    return new( GAME_HEAP, GAME_HEAP_FLAGS ) CEnemySpawn( packet );
}
