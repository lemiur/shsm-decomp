#ifndef CLIMAX_AUDIO_AUDIOMANAGER_H
#define CLIMAX_AUDIO_AUDIOMANAGER_H

#include <revolution/types.h>
#include <climax/Audio/EventGroup.h>

class unk_MusicPlayer;

namespace Audio {

class MixerSettings;
class CAudioRelay;
class StreamingSound;
class AutoDeleteSound;

/*
 * The base of the editor's items. Gormiti DWARF: a vtable only. Its fourth slot (0x0C) is the empty
 * function at 0x80039844 that the item vtables share (symbols.txt names it FreeMusicData__Q25Audio7ManagerFv).
 */
class CCustomItem {
public:
    virtual             ~CCustomItem() {}
    virtual void        Slot0C();
};

/* Gormiti DWARF: the item of one event group (size 0x8). TextFunc writes the status line. */
class SoundGroupItem : public CCustomItem {
public:
    virtual             ~SoundGroupItem();
    virtual int         TextFunc( char* buf );
    virtual void        EditFunc( int );

    EventGroup*         egroup;                 // 0x04
};

}

void fn_80038DB8( unk_MusicPlayer*, const char*, float );
void fn_80038FF4( unk_MusicPlayer*, const char*, float, int, int, int, int );

extern "C" {
void fn_80033EC4();
void fn_80033FD8();
void fn_80034008();
void fn_80034138();
void fn_800341C0();
void fn_800349EC();
void fn_80034A0C();
void fn_80034FDC();
void fn_80034FE0();
void fn_80035050();
void fn_800350E0();
void fn_8003517C();
void fn_80035244();
void fn_8003525C();
void fn_800352B8();
void fn_800352D0();
void fn_800353C8();
void fn_800353FC();
void fn_80035718();
void fn_8003584C();
void fn_80035B34();
void fn_80035D9C();
void fn_80035E44();
void fn_80036240();
void fn_80036268();
void fn_80036364();
void fn_800365F0();
void fn_80036A70();
void fn_80036B20();
void fn_80036C68();
void fn_80036D78();
void fn_80036D84();
void fn_80037018();
void fn_8003708C();
void fn_800371BC();
void fn_800373BC();
void fn_800374F4();
void fn_800376FC();
void fn_800378D8();
void fn_80037AD4();
void fn_80037C6C();
void fn_800380D4();
void fn_80038240();
void fn_800383C4();
void fn_800384B8();
void fn_800385F8();
void fn_8003885C();
void fn_80038C00();
void fn_80038C0C();
void fn_80039128();
void fn_80039230();
void fn_80039294();
void fn_800392A0();
void fn_800392BC();
void fn_80039350();
void fn_800394A4();
void fn_80039514();
void fn_80039704();
}

#endif
