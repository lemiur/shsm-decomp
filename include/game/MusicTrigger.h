#ifndef GAME_MUSICTRIGGER_H
#define GAME_MUSICTRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>
#include <game/CGenericSpawner.h>

/* The music player behind lbl_805292F8 (unit at 0x80033D94 not yet reconstructed). Name invented. */
struct unk_MusicPlayer;

extern unk_MusicPlayer* lbl_805292F8;

// defined in 0x80038DB8 and 0x80038FF4, unit not yet reconstructed: start the named track (a NULL
// or empty name stops the music); the float and the four flags are unknown
void    fn_80038DB8( unk_MusicPlayer* player, const char* name, float arg );
void    fn_80038FF4( unk_MusicPlayer* player, const char* name, float arg, int flag0, int flag1, int flag2,
                     int flag3 );

/*
 * Plays a music track when its message arrives ("MusicTrigger[%s]: Play %s"), or stops the music
 * when it names none; the track is cached while the trigger is active. The message, the track and
 * the float handed to the player are set by the attribute packet. The class name has no C prefix
 * in the registration strings. vtable __vt__12MusicTrigger, size 0x48. Member names invented.
 */
class MusicTrigger : public CBaseBehaviour, public unk_SpawnerResource {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetTrack,       // text: the track to play; empty stops the music
        CMD_SetPlayMsg,     // text: the name of the message that plays the track
        CMD_SetField3C      // float: field_0x3C
    };

                    MusicTrigger();
    virtual         ~MusicTrigger();
    virtual void    fn_8000CFD0();
    virtual void    fn_8000CFCC();
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual void    fn_8009CB1C( unk_Msg& msg );

    static CAttributeHandler* fn_800CC888( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) MusicTrigger;
    }

    char*           track;          // 0x34, owned copy; NULL when none
    void*           cacheHandle;    // 0x38, the request that caches track; NULL when none
    float           field_0x3C;     // 0x3C, handed to the player with the track
    unk_801A92A8    playMsg;        // 0x40
};

extern u32 lbl_80529B60;            // class-name hash of "MusicTrigger"
extern unk_801A8F6C lbl_80529B64;   // type-registry entry of MusicTrigger

#endif
