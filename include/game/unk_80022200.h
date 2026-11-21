#ifndef GAME_UNK_80022200_H
#define GAME_UNK_80022200_H

#include <string.h>
#include <revolution/types.h>
#include <game/AtomicTags.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>
#include <climax/Heap.h>

struct unk_80022200Item;
struct RpAtomic;
struct RpClump;
extern "C" void fn_800221F8( s32 param, RpAtomic* atomic, f32 dt );

// unk_80022200Task: name invented (no class-name evidence in this range). Its layout is read from
// fn_800227AC (initialiser), fn_800227F8 (destructor) and fn_8002273C (signal). Member names invented.
struct unk_80022200Task {
    unk_80022200Item**  field_0x0;      // owned list: begin
    unk_80022200Item**  field_0x4;      // owned list: end
    unk_80022200Item**  field_0x8;      // owned list: storage end
    u32                 field_0xC;      // 0 until signalled
    u32                 field_0x10;     // state; 3 once signalled
    s32                 field_0x14;     // id; -1 by default
    f32                 field_0x18;     // scale; 1.0f by default
    RpClump*            field_0x1C;     // owner clump
    void                (*field_0x20)( unk_80022200Task*, s32, void* );  // callback
    void*               field_0x24;     // callback argument
    f32                 field_0x28;
    f32                 field_0x2C;
    f32                 field_0x30;
    u8*                 field_0x34;     // owned byte array (delete[])
};

// unk_80022200Item: name invented. One entry of the owned list, 0x120 bytes (fn_80022900).
struct unk_80022200Item {
    char                    name[0x80];         // 0x00: the full name as given
    u32                     nameHash;           // 0x80: hash of name
    char                    baseName[0x80];     // 0x84: the part of name before its first '.'
    u32                     baseHash;           // 0x104: hash of baseName
    AtomicTags::E_AtomicType atomicType;        // 0x108: AtomicTags::Invalid until read from baseName
    RpAtomic*               atomic;             // 0x10C: FindAtomicByType( owner, atomicType )
    s32                     param;              // 0x110: the argument of the add call
    s32                     id;                 // 0x114: -1 by default
    void                    (*updateCallback)( s32 param, RpAtomic* atomic, f32 dt );  // 0x118: fn_800221F8
    RpAtomic*               (*setAnimation)( RpAtomic*, RpDMorphAnimation* );  // 0x11C: RpDMorphAtomicSetAnimation

    unk_80022200Item() : updateCallback( NULL ), setAnimation( NULL ) {
        memset( this, 0, sizeof( *this ) );
        atomicType = AtomicTags::Invalid;
        id = -1;
        updateCallback = fn_800221F8;
        setAnimation = RpDMorphAtomicSetAnimation;
    }
};

extern "C" {
void fn_80022200( RpAtomic* atomic );
AtomicTags::E_AtomicType fn_8000C5C4( const char* name, int length );
unk_80022200Task* fn_8002273C( unk_80022200Task* self, unk_80022200Task* other );
void fn_800227AC( unk_80022200Task* self, RpClump* owner );
void fn_800227F8( unk_80022200Task* self, s32 deleteThis );
s32 fn_80022900( unk_80022200Task* self, const char* name, s32 param );
void fn_80022C50();
void fn_80022F1C();
bool fn_8002304C( unk_80022200Task* self, u32 id );
void fn_80023100();
void fn_800231D4( unk_80022200Task* self, u32 index );
void fn_80023548( unk_80022200Task* self, u32 index, s32 value );
bool fn_8002355C( unk_80022200Task* self, u32 id );
s32 fn_800235D8( unk_80022200Task* self, const char* name );
void fn_80023694();
void fn_80023890();
void fn_800239DC( unk_80022200Task* self, u32 signalled );
void fn_800239E4();
void fn_80023B30();
void fn_80023C64();
void fn_80023DA0( unk_80022200Task* self, f32 scale );
void fn_80023DA8( unk_80022200Task* self, f32 dt );
void fn_80023E60();
}

#endif
