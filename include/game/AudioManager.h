#ifndef GAME_AUDIOMANAGER_H
#define GAME_AUDIOMANAGER_H

// Names and layout: Gormiti link map and DWARF (AudioManager.obj: CCustomItem, Audio::MixerItem,
// Audio::SoundSourceItem). Retail's functions pair with Gormiti's at the same instruction sequence.

#include <revolution/types.h>
#include <climax/Audio/Source.h>

class CCustomItem {
public:
    virtual             ~CCustomItem() {}
    virtual void        TextFunc( char*, int ) = 0;
    virtual void        EditFunc( int ) = 0;
};

namespace Audio {

class MixerSettings;

/* An item that holds the mixer settings it edits. */
class MixerItem : public CCustomItem {
public:
    virtual             ~MixerItem();   // override
    virtual void        TextFunc( char*, int );
    virtual void        EditFunc( int );

    MixerSettings*      mixer;          // 0x04
};

/* An item that toggles playback of one sound source. */
class SoundSourceItem : public CCustomItem {
public:
    virtual             ~SoundSourceItem();   // override
    virtual void        TextFunc( char*, int );
    virtual void        EditFunc( int );

    ISource*            soundSource;    // 0x04
    int                 index;          // 0x08
};

}  // namespace Audio

#endif
