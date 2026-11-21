// Unit name and boundary: Gormiti link map (game AudioManager.obj)

#include <game/AudioManager.h>
#include <game/MusicTrigger.h>

/*
====================
Audio::MixerItem::EditFunc

Hands the item's mixer settings to the music player; an item with no mixer does nothing.
====================
*/
// 0x80033DEC
void Audio::MixerItem::EditFunc( int ) {
    if ( mixer != NULL ) {
        fn_80039350( lbl_805292F8, mixer, 1, 1 );
    }
}

/*
====================
Audio::MixerItem::TextFunc
====================
*/
// 0x80033E0C
void Audio::MixerItem::TextFunc( char*, int ) {
}

/*
====================
Audio::SoundSourceItem::EditFunc

Toggles the source: a playing or paused source is stopped, a stopped one is played on the free channel.
====================
*/
// 0x80033E10
void Audio::SoundSourceItem::EditFunc( int ) {
    if ( soundSource->IsPlaying() == 1 ) {
        soundSource->Stop( true );
    } else if ( soundSource->IsPaused() == 1 ) {
        soundSource->Stop( true );
    } else {
        soundSource->Play( Audio::Channel::Free, false );
    }
}
