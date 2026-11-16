#ifndef GAME_CPOINTOFINTEREST_H
#define GAME_CPOINTOFINTEREST_H

// Unit name: the class name string "CPointOfInterest" (0x803EBAD0), which the unit's static
// initialiser hashes into lbl_80529A80, the class id of the attribute commands of fn_801A73A8.

#include <climax/AttributeHandler.h>
#include <climax/Frame.h>
#include <game/VectorPS.h>

class CBaseBehaviour;
class unk_8003C454;

/*
 * The abstract base of the objects the game points at: triggers, spawned characters, hiding
 * places (vtable lbl_803EBA7C, size 0x44). Each point is filed with the point manager
 * lbl_80529A88 while it lives. It is a line segment: the owner's frame position plus offset is
 * the centre, extent the half length along the frame's right vector. Slots are named after
 * their first implementation; the pure ones after CPointTrigger's. Member names invented.
 */
class CPointOfInterest {
public:
    /* Attribute commands. Names invented. */
    enum {
        CMD_SetName,            // text: kept in the packet
        CMD_SetField0xC,        // float
        CMD_SetOffset,          // three floats
        CMD_SetField0x1C,       // integer: nonzero sets the flag
        CMD_SetField0x1D,       // integer: nonzero sets the flag
        CMD_SetField0x20,       // float
        CMD_SetExtent,          // float
        CMD_SetField0x28,       // float
        CMD_SetSaveDataA,       // text: copied to saveDataA
        CMD_SetSaveDataB,       // text: copied to saveDataB
        CMD_SetField0x40,       // text: copied to field_0x40
        CMD_SetField0x2C,       // float
        CMD_SetField0x34,       // float
        CMD_SetField0x30        // integer
    };

                        CPointOfInterest( const unk_AttributePacket& packet );
    virtual             ~CPointOfInterest();
    virtual SClassInfo* fn_800C4E9C() = 0;
    virtual CZone*      fn_800C4E8C() = 0;
    virtual CZone*      fn_800C4E94() = 0;
    virtual void        fn_801A73A8( const unk_AttributePacket& packet );
    virtual void        fn_800C6274( unk_8003C454* who ) = 0;
    virtual int         fn_800C54EC();
    virtual bool        fn_8002CDF0();
    virtual bool        fn_8005230C();
    virtual bool        fn_800522FC();
    virtual unk_Vector3  fn_800C5280( const CBaseBehaviour* behaviour );
    virtual void        fn_800C53F4( const unk_RwV3d* point, unk_RwV3d* out );
    virtual void        fn_8002CDD0( CPointOfInterest* other, unk_Vec3* out );
    virtual bool        fn_8002CDC8();
    virtual bool        fn_8002CDA4();
    virtual float       fn_800522F4();
    virtual char*       fn_8002CD9C();
    virtual bool        fn_800C54E4();
    virtual float       fn_800C5320( const unk_RwV3d* point );

    void                fn_800C5050( const unk_AttributePacket& packet, bool addToManager );
    float               fn_800C52D8( const CBaseBehaviour* behaviour );

    unk_RwMatrix*       matrix;         // 0x4, the owner's frame matrix
    const char*         name;           // 0x8, in the attribute packet
    float               field_0xC;
    unk_Vector3         offset;         // 0x10, from the matrix position
    bool                field_0x1C;
    bool                field_0x1D;
    float               field_0x20;
    float               extent;         // 0x24, half length along the matrix's right vector
    float               field_0x28;
    float               field_0x2C;
    s32                 field_0x30;     // priority: the manager chooses the highest (fn_800C5800)
    float               field_0x34;
    char*               saveDataA;      // 0x38, owned copy; first argument of fn_80206348
    char*               saveDataB;      // 0x3C, owned copy; second argument of fn_80206348
    char*               field_0x40;     // 0x40, owned copy; a third save-data key
};

extern const char* lbl_80527498;    // "SaveData", the group of the save-data value
extern u32 lbl_80529A80;            // class-name hash of "CPointOfInterest"

#endif
