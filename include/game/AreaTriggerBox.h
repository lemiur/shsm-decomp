#ifndef GAME_AREATRIGGERBOX_H
#define GAME_AREATRIGGERBOX_H

// unit name unconfirmed: the class names are the registration strings and the log strings
// "AreaTriggerBox[%s] Sends %s (Enter Event)" and "(Exit Event)"; the file name is invented.

#include <climax/Memory.h>
#include <game/AreaTriggerBase.h>
#include <game/SaveData.h>

/*
 * The box shape of an area trigger: the unit cube centred on the area's frame (vtable
 * __vt__19AreaTriggerBoxLogic, size 0xD0). Still abstract: it leaves the enter and exit handlers to
 * AreaTriggerBox. Its name is the second class-name hash of the unit's static initialiser
 * (lbl_80529C38); that the hash names this class is inferred. Member names invented.
 */
class AreaTriggerBoxLogic : public AreaTriggerBase {
public:
                    AreaTriggerBoxLogic( const unk_AttributePacket& packet ) : AreaTriggerBase( packet ) {}
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~AreaTriggerBoxLogic();
    virtual bool    fn_800D1608( const unk_Vec3* point );
    virtual bool    fn_800D1688( const unk_Vec3* segment );
    virtual void    fn_800D1818();

    enum { NUM_FACES = 6 };
};

/*
 * A box area that sends one message when something enters it and another when it leaves, as
 * long as its save-data value (fn_80206348) is set. vtable __vt__14AreaTriggerBox, size 0xEC. Member
 * names invented.
 */
class AreaTriggerBox : public AreaTriggerBoxLogic {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetEnterMsg,        // text: the message sent on entering
        CMD_SetExitMsg,         // text: the message sent on leaving
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB,       // text: copied to saveDataB
        CMD_SetField0xE8        // text: copied to field_0xE8
    };

                    AreaTriggerBox( const unk_AttributePacket& packet ) :
                        AreaTriggerBoxLogic( packet ),
                        saveDataA( NULL ),
                        saveDataB( NULL ),
                        field_0xE8( NULL ) {}
    virtual void    fn_801A73A8( const unk_AttributePacket& packet );
    virtual         ~AreaTriggerBox();
    virtual void    fn_800D1434( void* data );
    virtual void    fn_800D14F0( void* data );

    static CAttributeHandler* fn_800D1890( const unk_AttributePacket& packet ) {
        return new( GAME_HEAP, GAME_HEAP_FLAGS ) AreaTriggerBox( packet );
    }

    unk_801A92A8    enterMsg;       // 0xD0
    unk_801A92A8    exitMsg;        // 0xD8
    char*           saveDataA;      // 0xE0, owned copy; first argument of fn_80206348
    char*           saveDataB;      // 0xE4, owned copy; second argument of fn_80206348
    char*           field_0xE8;     // 0xE8, owned copy; a third save-data key whose test is always true here

private:
    // The test of the third key, whose debug check is compiled out in release. Name invented.
    bool            CheckKey() {
        if ( field_0xE8 != NULL ) {
            return CheckSaveDataKey( field_0xE8 );
        }
        return true;
    }
};

extern const char* lbl_80527578;    // "SaveData", the group of the save-data value
extern u32 lbl_80529C30;            // class-name hash of "AreaTriggerBox"
extern unk_801A8F6C lbl_80529C34;   // type-registry entry of AreaTriggerBox
extern u32 lbl_80529C38;            // class-name hash of "AreaTriggerBoxLogic"

#endif
