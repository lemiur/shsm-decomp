// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80098F34.h>
#include <game/MusicTrigger.h>
#include <game/unk_PromptManager.h>
#include <game/unk_800E671C.h>
#include <game/unk_RespotManager.h>
#include <climax/Localisation.h>
#include <string.h>
#include <stdio.h>

/*
====================
unk_803E8A68::~unk_803E8A68

Unregisters the two messages the listener subscribed to. The base destructor and the delete
are the compiler's.
====================
*/
// 0x80098F34
unk_803E8A68::~unk_803E8A68() {
    fn_801A9E00( lbl_8052A828 );
    fn_801A9E00( lbl_8052A7B0 );
}

/*
====================
unk_803E8A68::fn_80098FB0

Finds the file called fileName under the file system's root and opens its resource.
With flag 4 the music player is paused and the game loop is told a load runs. When the resource
does not open, the music resumes and the load ends. Returns whether the file was found.
====================
*/
// 0x80098FB0
bool unk_803E8A68::fn_80098FB0( const char* fileName, int flags ) {
    if ( lbl_8052987C != 0 ) {
        return false;
    }
    this->flags = flags;
    field_0x3D = 0;
    if ( flags & 0x4 ) {
        if ( musicPaused == 0 ) {
            fn_8003525C( lbl_805292F8 );
            fn_800352B8( lbl_805292F8 );
            musicPaused = 1;
        }
        fn_80098F2C( true );
    }
    char pattern[ 0x100 ];
    char found[ 0x100 ];
    sprintf( pattern, "%s%s%s", RWS::FileSystem::GetDir( RWS::FileSystem::eDir_Root ), fileName, "" );
    char* result = fn_801D207C( pattern, found, NULL, 1 );
    active = false;
    if ( result != NULL ) {
        strcpy( name, fileName );
        resource = fn_80099AB4( result, this->flags );
        if ( resource != NULL ) {
            if ( lbl_80529C78 != NULL ) {
                lbl_80529C78->fn_800D838C( 5.0f, fileName, false, false );
            }
            RWS::MainLoop::System::SetSystemState( RWS::MainLoop::System::eSystemState_Zero, true );
            savedFlag = lbl_80529E44->field_0x252[0];
            lbl_80529E44->field_0x252[0] = 0;
        } else {
            if ( musicPaused != 0 ) {
                fn_80035244( lbl_805292F8 );
                fn_80035050( lbl_805292F8 );
                musicPaused = 0;
            }
            fn_80098F2C( false );
            active = true;
        }
    }
    return result != NULL;
}

/*
====================
unk_803E8A68::fn_8009CB1C

Message handler. The message lbl_8052A7B0 marks the listener active. The message lbl_8052A828
ends a loaded resource once fn_800999A8 allows it: the game state gets its saved flag back,
the resource is released, the music resumes and the end message is posted when it is subscribed.
====================
*/
// 0x80099140
void unk_803E8A68::fn_8009CB1C( unk_Msg& msg ) {
    if ( msg.record == lbl_8052A7B0.record ) {
        active = true;
        return;
    }
    if ( msg.record != lbl_8052A828.record ) {
        return;
    }
    if ( resource == NULL ) {
        return;
    }
    if ( fn_800999A8( field_0x3D ) == false ) {
        return;
    }
    if ( resource != NULL ) {
        if ( field_0x3D != 0 ) {
            field_0x3D = 0;
            fn_80099A78( false );
        }
        lbl_80529E44->field_0x252[0] = savedFlag;
        RWS::MainLoop::Render::SetNoFlip();
        fn_80099A7C( resource );
        resource = NULL;
        if ( musicPaused != 0 ) {
            fn_80035244( lbl_805292F8 );
            fn_80035050( lbl_805292F8 );
            musicPaused = 0;
        }
        fn_80098F2C( false );
        fn_80030F00( lbl_80529260, true );
        RWS::MainLoop::System::SetSystemState( RWS::MainLoop::System::eSystemState_Zero, false );
        if ( lbl_80529758.record != lbl_8052A530.record && lbl_80529758.record->head.next != NULL ) {
            unk_Msg done( lbl_80529758, NULL );
            fn_801AA2AC( done );
        }
    }
    active = false;
}
