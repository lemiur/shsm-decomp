// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8001238C.h>
#include <math.h>
#include <rwsdk/badevice.h>
#include <climax/Heap.h>

// Data owned by this unit (declared in the header, defined here under their symbols.txt names):
const f32 lbl_8052B768 = 0.0f;
const f32 lbl_8052B76C = 0.001f;

/*
====================
fn_8001238C
====================
*/
// 0x8001238C
// Constructor: zeroes the state, takes the shared object from fn_801874FC or allocates a new one
// through fn_80011BB0, and builds the name table from the shared object's name list.
// Wall (92.81%): the shared-object reference count and the NULL branch are laid out differently
// (retail keeps the NULL store as the fall-through and the count increment as the branch
// target), and the loop's register numbering differs; every store, call and constant matches.
extern "C" void fn_8001238C(unk_8001238C* self, void* arg, void* arg2) {
    self->field_0x0 = NULL;
    self->field_0x8.f = lbl_8052B768;
    self->field_0xc.f = lbl_8052B768;
    self->field_0x10 = lbl_8052B768;
    self->field_0x14 = lbl_8052B768;
    self->field_0x18 = lbl_8052B768;
    self->field_0x1c = 1;
    self->field_0x20 = 0;
    self->field_0x4 = arg2;

    unk_Shared* shared = (unk_Shared*)fn_801874FC(arg);
    if (shared == NULL) {
        void* block = operator new(sizeof(unk_Shared), GAME_HEAP, GAME_HEAP_FLAGS);
        if (block != NULL) {
            shared = fn_80011BB0(block, arg);
        }
    }
    if (shared != NULL) {
        shared->field_0x30++;
    }
    self->field_0x28 = shared;

    s32 count = shared->field_0x28;
    if (count != 0) {
        self->field_0x24 = new (GAME_HEAP, GAME_HEAP_FLAGS) unk_NameHashEntry[count];
        for (s32 i = 0; i < count; i++) {
            unk_NameHashEntry* entry = &self->field_0x24[i];
            const char* name = self->field_0x28->field_0x2c[i];
            if (name != NULL && name[0] != '\0') {
                entry->field_0x0 = name;
                entry->field_0x4 = RWS::SHUtils::GetStringHashForceLower(name, -1);
                entry->field_0x8 = 0;
            }
        }
    } else {
        self->field_0x24 = NULL;
    }

    self->field_0x0 = self->field_0x28->field_0x0->field_0x4[*self->field_0x28->field_0xc];
}

/*
====================
__ct__13unk_NameEntryFv
====================
*/
// 0x80012510
// Element constructor of the name table: clears the name and words 8 and 12.
unk_NameHashEntry::unk_NameHashEntry() {
    field_0x0 = NULL;
    field_0x8 = 0;
    field_0xc = 0;
}

/*
====================
fn_80012524
====================
*/
// 0x80012524
// Destructor: drops the reference on the shared object (freeing it with its resource handle
// on the last one), frees the name table, and frees the object too when deleteFlag is positive.
// The name table's element type has a constructor and no destructor, so delete[] takes the
// array cookie (0x10 bytes) back before __dla; its null test is the delete[]'s own.
extern "C" unk_8001238C* fn_80012524(unk_8001238C* self, s32 deleteFlag) {
    if (self != NULL) {
        unk_Shared* shared = self->field_0x28;
        s32 refs = shared->field_0x30 - 1;
        shared->field_0x30 = refs;
        if (refs <= 0) {
            if (shared != NULL) {
                RwFree(shared->field_0x34);
                delete shared;
            }
        }
        delete[] self->field_0x24;
        if (deleteFlag > 0) {
            delete self;
        }
    }
    return self;
}

/*
====================
fn_800125C8
====================
*/
// 0x800125C8
// Returns the item whose name hash equals the hash of name, or NULL.
extern "C" unk_Named* fn_800125C8(unk_Shared* self, const char* name) {
    unk_Named* found = NULL;
    u32 hash = RWS::SHUtils::GetStringHashForceLower(name, -1);
    s32 i = 0;
    while (i < self->field_0x18) {
        if (fn_8000F8F0(self->field_0x4[i])->field_0x0 == hash) {
            found = self->field_0x4[i];
            break;
        }
        i++;
    }
    return found;
}

/*
====================
fn_80012650
====================
*/
// 0x80012650
// Returns item[index], or NULL when the table is empty or the index is out of range.
// Wall (80.91%): retail lays out the in-range return before the shared NULL return; the
// early-return form reproduces the instructions but not their block order.
extern "C" unk_Named* fn_80012650(unk_Shared* self, s32 index) {
    if (self->field_0x4 == NULL) {
        return NULL;
    }
    if (index >= self->field_0x18) {
        return NULL;
    }
    return self->field_0x4[index];
}

/*
====================
fn_8001267C
====================
*/
// 0x8001267C
extern "C" s32 fn_8001267C(unk_8001238C* self, u32 a, u32 b) {
    return fn_80010B94(self->field_0x0, self, a, b);
}

/*
====================
fn_80012698
====================
*/
// 0x80012698
extern "C" s32 fn_80012698(unk_8001238C* self, u32 a) {
    return fn_80010E2C(self->field_0x0, self, a);
}

/*
====================
fn_800126AC
====================
*/
// 0x800126AC
extern "C" void fn_800126AC(unk_8001238C* self, f32 delta) {
    self->field_0xc.f = delta;
    self->field_0x8.f += delta;
}

/*
====================
fn_800126C0
====================
*/
// 0x800126C0
extern "C" s32 fn_800126C0(unk_8001238C* self) {
    unk_Source* source = (unk_Source*)fn_80010FC4(self->field_0x0, self);
    if (source != NULL) {
        unk_Shared* shared = source->field_0x28;
        fn_80010E28(self->field_0x0);
        self->field_0x0 = shared;
        self->field_0x8.f = 0.0f;
        self->field_0x20 = 0;
        self->field_0x10 = source->field_0x20;
        self->field_0x1c = source->field_0x24;
        return 1;
    }
    return 0;
}

/*
====================
fn_8001274C
====================
*/
// 0x8001274C
// Looks up name in the shared object's item table. When found, makes that item current and
// sets the timing fields from value (value <= 0 sets field_0x1c); returns 1 on success.
// Wall (93.69%): control flow (retail branches on value <= 0 with the zero it stored earlier;
// the compiler materialises the flag) and register/schedule (the search loop's counter, offset
// and found registers); the field stores all match.
extern "C" s32 fn_8001274C(unk_8001238C* self, const char* name, f32 value) {
    unk_Named* found = NULL;
    unk_Shared* shared = self->field_0x28;
    u32 hash = RWS::SHUtils::GetStringHashForceLower(name, -1);
    s32 i;
    for (i = 0; i < shared->field_0x18; i++) {
        if (fn_8000F8F0(shared->field_0x4[i])->field_0x0 == hash) {
            found = shared->field_0x4[i];
            break;
        }
    }
    if (found != NULL) {
        fn_80010E28(self->field_0x0);
        self->field_0x0 = found;
        self->field_0x8.f = 0.0f;
        self->field_0x20 = 0;
        self->field_0x10 = value;
        s32 holdFlag = 0;
        if (value <= 0.0f) {
            holdFlag = 1;
        }
        self->field_0x1c = holdFlag;
        self->field_0x20 = 0;
        return 1;
    }
    return 0;
}

/*
====================
fn_80012840
====================
*/
// 0x80012840
// Stores value at field_0x14 and the previous target at field_0x18. A negative target
// means "hold the current value"; a target within 0.001 of the current value snaps back to it.
// Wall (82.50%): retail rounds the selected target to single (frsp) before the difference; the
// ternary form does not emit that rounding.
extern "C" void fn_80012840(unk_8001238C* self, f32 value, f32 target) {
    f32 previous = (target >= 0.0f) ? target : self->field_0x14;
    self->field_0x18 = previous;
    f32 diff = fabs(previous - self->field_0x14);
    if (diff < 0.001f) {
        self->field_0x18 = self->field_0x14;
    }
    self->field_0x14 = value;
}

/*
====================
fn_80012888
====================
*/
// 0x80012888
extern "C" void fn_80012888(unk_8001238C* self) {
    self->field_0x20 = 0;
}

/*
====================
fn_80012894
====================
*/
// 0x80012894
// Destructor: frees the entry array, and the object too when deleteFlag is positive.
extern "C" unk_PairTable* fn_80012894(unk_PairTable* self, s32 deleteFlag) {
    if (self != NULL) {
        if (self->field_0x0 != NULL) {
            delete[] self->field_0x0;
            self->field_0x0 = NULL;
            self->field_0x4 = 0;
            self->field_0x8 = 0;
        }
        if (deleteFlag > 0) {
            delete self;
        }
    }
    return self;
}

/*
====================
fn_80012904
====================
*/
// 0x80012904
// Adds key with value unless key is already in the table. The slot past the last
// entry takes the key first, so the scan always stops.
extern "C" void fn_80012904(unk_PairTable* self, u32 key, u32 value) {
    s32 i = 0;
    self->field_0x0[self->field_0x4].field_0x0 = key;
    unk_PairEntry* entry = self->field_0x0;
    while (entry->field_0x0 != key) {
        entry++;
        i++;
    }
    if (i != self->field_0x4) {
        return;
    }
    self->field_0x4++;
    self->field_0x0[i].field_0x4 = value;
}

/*
====================
fn_8001295C
====================
*/
// 0x8001295C
// Reverse lookup: returns the word-0 field of the first entry whose word-4 field equals
// value, or 0 when none does. The slot past the last entry is the sentinel.
// Wall: register/schedule (retail keeps the base in r5 and the walker in r3; the
// declaration orders tried so far give the opposite assignment).
extern "C" u32 fn_8001295C(unk_PairTable* self, u32 value) {
    s32 i;
    self->field_0x0[self->field_0x4].field_0x0 = 0;
    i = 0;
    self->field_0x0[self->field_0x4].field_0x4 = value;
    unk_PairEntry* entry = self->field_0x0;
    unk_PairEntry* entries = entry;
    while (entry->field_0x4 != value) {
        entry++;
        i++;
    }
    return entries[i].field_0x0;
}
