// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_PhysicsManager.h>
#include <game/unk_8000D090.h>

/*
====================
unk_PhysicsManager::fn_8000F71C

Removes the body from the world it is placed in, when the world's object table holds the body
at the body's index. Returns true when a body was removed. The manager is not used.
====================
*/
// 0x8000F71C
bool unk_PhysicsManager::fn_8000F71C( unk_RwpBody* body ) {
    bool removed = false;
    if ( body != NULL ) {
        unk_RwpWorld* world = body->world;
        if ( world != NULL && body->index < world->objectCount && world->objects[ body->index ] == body ) {
            RwpMgrRemoveObj( world, body );
            lbl_80529178--;
            removed = true;
        }
    }
    return removed;
}

/*
====================
fn_8000F78C

Stub: creates a 0x10C-byte object from the heap and constructs it with four arguments, the
manager's member at 0xC and the manager. The parameter types and the object's class are not
yet known (its constructor is fn_8000D090, in another unit).
====================
*/
// 0x8000F78C
extern "C" void fn_8000F78C() {
    // TODO identity: parameters and the created object's class not yet known
}

/*
====================
fn_8000F7F8

Deletes the object given as the second argument: a tail call of the deleting destructor
of unk_8000D090 with the delete flag. The first argument is the manager (not used). The object
(0x10C bytes) is the one fn_8000F78C creates.
====================
*/
// 0x8000F7F8
extern "C" void fn_8000F7F8( void* manager, unk_8000D090* object ) {
    delete object;
}

/*
====================
fn_8000F804

Returns 0. Its owner is not known; no caller reads a value other than 0 from it.
====================
*/
// 0x8000F804
extern "C" int fn_8000F804() {
    // TODO identity: owning class not yet known
    return 0;
}
