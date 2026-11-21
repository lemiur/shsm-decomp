#ifndef GAME_UNK_80098F34_H
#define GAME_UNK_80098F34_H

// unit name unconfirmed: header named after the unit's lowest function (no class-name evidence in
// this unit; the class is named by its vtable address, name invented).

#include <climax/EventHandler.h>
#include <game/Messages.h>

/* Declared here because the unit's code calls them; defined in units not yet reconstructed. */
extern "C" {
// defined in 0x80098F2C, unit not yet reconstructed: stores the flag that the game loop reads
void            fn_80098F2C( bool flag );
// defined in 0x800999A8, unit not yet reconstructed: returns true when its flag is set
bool            fn_800999A8( u8 flag );
// defined in 0x80099A78, unit not yet reconstructed: takes the flag
void            fn_80099A78( bool flag );
// defined in 0x80099A7C, unit not yet reconstructed: releases the resource
void            fn_80099A7C( void* resource );
// defined in 0x80099AB4, unit not yet reconstructed: opens the resource at path
void*           fn_80099AB4( const char* path, int flags );
// defined in 0x8003525C, 0x800352B8, 0x80035244, 0x80035050: the music player's pause and resume
void            fn_8003525C( void* player );
void            fn_800352B8( void* player );
void            fn_80035244( void* player );
void            fn_80035050( void* player );
// defined in 0x80030F00, unit not yet reconstructed: sets the flag of the respot manager
void            fn_80030F00( void* manager, bool flag );
}

// defined in 0x801B323C, unit not yet reconstructed: the directory of a file-system root
namespace RWS {
namespace FileSystem {
enum eDir {
    eDir_Root = 0   // the constant the code uses; the others are not recovered
};
const char*     GetDir( eDir dir );
}
namespace MainLoop {
class System {
public:
    enum eSystemState {
        eSystemState_Zero = 0   // the constant the code uses; the others are not recovered
    };
    // defined at 0x80202F7C, unit not yet reconstructed
    static void SetSystemState( eSystemState state, bool flag );
};
class Render {
public:
    // defined at 0x8020449C, unit not yet reconstructed
    static void SetNoFlip();
};
}
}

// Globals this unit uses; each is defined in its owning unit.
extern int                  lbl_8052987C;   // nonzero: the load is refused
extern unk_801A92A8         lbl_80529758;   // the message that the load posts once it is registered

/*
 * A message listener that loads a file, its resource, when it is asked to, and posts
 * lbl_80529758 when the load ends. Name invented; the class is named by its vtable at 0x803E8A68.
 * Its constructor is inline in the static initialiser at 0x80099278 (next unit).
 */
class unk_803E8A68 : public CEventHandler {
public:
    virtual         ~unk_803E8A68();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    bool            fn_80098FB0( const char* fileName, int flags );

    void*           resource;           // 0x8, the resource fn_80098FB0 opened; NULL when none
    char            name[ 0x20 ];       // 0xC, the file name fn_80098FB0 took
    int             flags;              // 0x2C, the flags fn_80098FB0 took
    bool            active;             // 0x30, set when the load is running
    int             musicPaused;        // 0x34, the music player is paused by this object
    int             field_0x38;         // 0x38, set to 0x8000 by the constructor
    u8              savedFlag;          // 0x3C, the game state's flag at 0x252 while fn_80098FB0 ran
    u8              field_0x3D;         // 0x3D
};

#endif
