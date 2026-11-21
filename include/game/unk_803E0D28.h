#ifndef GAME_UNK_803E0D28_H
#define GAME_UNK_803E0D28_H

#include <revolution/types.h>

/*
 * A polymorphic game object (vtable __vt__12unk_803E0D28 at 0x803E0D28, 20 virtual slots, size 0x70).
 * Name invented: the class has no string evidence, so the placeholder is the vtable address. Its
 * factory is fn_8001ECC0; the derived class is unk_803E3450. Only the fields the first unit reads are
 * named; the rest are padding.
 */
class unk_803E0D28 {
public:
    virtual ~unk_803E0D28();                         // slot 0x08, deleting destructor fn_800202D0
    virtual void fn_8001B5DC();                      // slot 0x0C
    virtual void fn_8001B778();                      // slot 0x10
    virtual void* fn_80020368();                     // slot 0x14, returns field_0x60
    virtual void fn_8001FB08();                      // slot 0x18
    virtual void fn_8001EF44();                      // slot 0x1C
    virtual void fn_8001CD24();                      // slot 0x20
    virtual unsigned char fn_80020360();             // slot 0x24, returns field_0x69
    virtual void fn_8001CD1C();                      // slot 0x28
    virtual float fn_80020358();                     // slot 0x2C, returns field_0x64
    virtual void fn_8001B27C();                      // slot 0x30
    virtual void fn_8001B298();                      // slot 0x34
    virtual void fn_8001FAF4();                      // slot 0x38
    virtual void fn_80020370( u32 minValue, u32 maxValue );  // slot 0x3C, parameter names invented
    virtual void fn_8001B884();                      // slot 0x40
    virtual void fn_8001BA3C();                      // slot 0x44
    virtual void fn_8001C42C();                      // slot 0x48
    virtual void fn_8001C9B8();                      // slot 0x4C
    virtual void fn_8001B7AC( int arg );             // slot 0x50
    virtual void fn_8001CC1C();                      // slot 0x54

    u8      field_0x4[0x58];
    u32     field_0x5C;
    void*   field_0x60;
    float   field_0x64;
    u8      field_0x68;
    u8      field_0x69;
    u8      field_0x6A[0x6];
};

#endif
