#ifndef GAME_DATATRIGGER_H
#define GAME_DATATRIGGER_H

#include <climax/Memory.h>
#include <game/CBaseBehaviour.h>

// defined in 0x802061DC and 0x802061E8 (unit not yet reconstructed): the data store behind
// DataTrigger. fn_802061E8 sets the value of a key in a group ("SaveData"). Names unknown.
void    fn_802061DC( const char* value );
void    fn_802061E8( const char* key, const char* value, const char* group );

/*
 * Stores a value under a key in the "SaveData" group when its message arrives; the key, the
 * value and the message are named by the attribute packet (registered as "DataTrigger").
 * vtable __vt__11DataTrigger, size 0x48. Member names invented.
 */
class DataTrigger : public CBaseBehaviour {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetKey,         // text: the key
        CMD_SetValue,       // text: the value stored under it
        CMD_SetMsg          // text: the name of the message that stores it
    };

                    DataTrigger() {
                        key = NULL;
                        value = NULL;
                    }
    virtual         ~DataTrigger();
    virtual void    fn_8009CB1C( unk_Msg& msg );
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );

    static CAttributeHandler* fn_800C9378( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) DataTrigger;
    }

    char*           key;        // 0x34, owned copy
    char*           value;      // 0x38, owned copy
    u32             field_0x3C; // never used by the unit
    unk_801A92A8    msg;        // 0x40
};

extern const char* lbl_80527518;    // "SaveData", the group DataTrigger stores into
extern u32 lbl_80529AE8;            // class-name hash of "DataTrigger"
extern unk_801A8F6C lbl_80529AEC;   // type-registry entry of DataTrigger

#endif
