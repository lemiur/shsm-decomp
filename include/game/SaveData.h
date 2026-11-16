#ifndef GAME_SAVEDATA_H
#define GAME_SAVEDATA_H

// unit name unconfirmed: header name invented, after the "SaveData" group name the callers pass.

#include <revolution/types.h>

/*
 * Save data, defined in 0x80204C04-0x80206544 (units not yet reconstructed). A save block is
 * found by name in the root (fn_802050AC); named values are written (fn_80205354) and read
 * (fn_802057FC) with a type code and a byte size: CAmbientInterferencePoint passes type 1 with
 * one byte, CKeyHUDTrigger type 2 with four. Parameter names invented.
 */
void*   fn_80204C04();
void*   fn_802050AC( void* owner, const char* name, int a, int b, int c, int d );
void    fn_80205354( void* save, const char* name, int type, const void* value, int size );
void    fn_802057FC( void* save, const char* name, int type, void* value, int size );
// defined in 0x8020609C, unit not yet reconstructed: the save block group (and, when name is not
// NULL, its child name) found by fn_802050AC in the root, or NULL
void*   fn_8020609C( const char* group, const char* name, int a );
int     fn_80206348( const char* a, const char* b, const char* group );

/*
====================
CheckSaveDataKey

The debug check of a trigger's third save-data key, which follows the save-data test
fn_80206348 at every site. Compiled out in release: the test is true on both arms, and only its
dead compare of the key survives where a caller's own key test passes the key in (the user's
ruling in docs/exceptions.md, "Game code rulings (game-11)"). Callers pass the key field
itself. Name invented.
====================
*/
inline bool CheckSaveDataKey( const char* key ) {
    if ( key != NULL ) {
        return true;
    }
    return true;
}

#endif
