#ifndef GAME_UNK_8002A9A4_H
#define GAME_UNK_8002A9A4_H

// Unit name unconfirmed: no string or class-name evidence names these types. The class and
// member names are invented; the layouts are the ones the code of this unit reads and writes.

#include <revolution/types.h>
#include <climax/Heap.h>


class unk_RefList;

/*
 * The reference-counted object that fn_8012BDB0 constructs (unit auto_03_8012BD58). Named after
 * its constructor address (invented). Slot 2 of its vtable (lbl_803F6420) is the deleting
 * destructor: a call with flag 1 frees the object.
 */
class unk_8012BDB0 {
public:
    virtual ~unk_8012BDB0();
    virtual void    fn_8012BEF4();
    virtual void    fn_80133FB0();
    virtual void    fn_80134098();
    virtual void    fn_8012C4C4();
    virtual void    fn_8012C52C();

    s32             refCount;       // 0x4, starts at 0
    unk_RefList*    owner;          // 0x8, the list the constructor was given
    s32             field_0xC;      // 0xC
    s32             field_0x10;     // 0x10
    unk_8012BDB0*   partner;        // 0x14, the object built alongside this one
    float           field_0x18;     // 0x18, the constructor's first float argument
    s32             field_0x1C;     // 0x1C
    u32             field_0x20;     // 0x20, the constructor's second float argument times 1000
    s32             field_0x24;     // 0x24, the constructor's int argument
};

/* A counted pointer to unk_8012BDB0 (name invented): it holds one reference while it lives. */
class unk_Ref {
public:
                    unk_Ref( unk_8012BDB0* value ) {
                        ptr = value;
                        if ( ptr != NULL ) {
                            ptr->refCount++;
                        }
                    }
                    unk_Ref( const unk_Ref& other ) {
                        ptr = other.ptr;
                        if ( ptr != NULL ) {
                            ptr->refCount++;
                        }
                    }
                    ~unk_Ref() {
                        if ( ptr != NULL ) {
                            ptr->refCount--;
                            if ( ptr->refCount == 0 ) {
                                delete ptr;
                            }
                        }
                    }

    unk_8012BDB0*   ptr;
};

/* One node of unk_RefList. The node owns one reference to data. */
struct unk_RefNode {
                    unk_RefNode( unk_8012BDB0* value ) {
                        data = value;
                        if ( data != NULL ) {
                            data->refCount++;
                        }
                        prev = NULL;
                        next = NULL;
                    }

    unk_8012BDB0*   data;           // 0x0
    unk_RefNode*    prev;           // 0x4
    unk_RefNode*    next;           // 0x8
};

/*
 * A doubly linked list of owned references (name invented). Only the members at 0x14 to 0x1C
 * are read by this unit; the bytes before them belong to the list's owner.
 */
class unk_RefList {
public:
    u8              field_0x0[0x14];
    s32             count;          // 0x14
    unk_RefNode*    tail;           // 0x18
    unk_RefNode*    head;           // 0x1C
};

/* The name of one record, as its first halfword. */
struct unk_NameRecord {
    u16             nameIndex;      // 0x0
};

/* A cell of unk_NameIndex: points at the record it names. */
struct unk_NameCell {
    unk_NameRecord* record;         // 0x0
};

/* The header of unk_NameIndex: the cell count is the halfword at 0x2. */
struct unk_NameIndexHeader {
    u8              field_0x0[0x2];
    u16             count;          // 0x2
};

/*
 * A table of name cells and the name blob they index. The blob starts with an array of 32-bit
 * offsets, one per name index; each offset is from the blob start to a NUL-terminated name.
 */
class unk_NameIndex {
public:
    s32             field_0x0;      // 0x0
    unk_NameIndexHeader* header;    // 0x4
    unk_NameCell**  cells;          // 0x8
    char*           blob;           // 0xC
};

/* One 0x18-byte entry of unk_NamedEntryList: its name index is the first halfword. */
struct unk_NamedEntryRecord {
    u16             nameIndex;      // 0x0
    u8              field_0x2[0x16];
};

/* The header of unk_NamedEntryList: the entry count is the halfword at 0x8. */
struct unk_EntryListHeader {
    u8              field_0x0[0x8];
    u16             count;          // 0x8
};

/* A list of entries whose names are in a name blob (name invented). */
class unk_NamedEntryList {
public:
    unk_EntryListHeader* header;    // 0x0
    u8              field_0x4[0xC];
    unk_NamedEntryRecord*  entries;        // 0x10
};

/* The name blob that unk_NamedEntryList lookups read (name invented). */
class unk_NameBlob {
public:
    u8              field_0x0[0xC];
    char*           blob;           // 0xC
};

extern "C" {
unk_8012BDB0*   fn_8012BDB0( unk_8012BDB0* self, s32 kind, float f1, float f2, unk_RefList* owner );
void            fn_8002A9A4( void* unused0, s32 unused1, float f1, unk_RefList* first, unk_RefList* second );
void            fn_8002AD78( unk_RefList* list, void* unused, unk_8012BDB0** source );
unk_NameCell*   fn_8002AE2C( unk_NameIndex* table, const char* name );
unk_NamedEntryRecord*  fn_8002AEF0( unk_NamedEntryList* list, unk_NameBlob* names, const char* name );
}

#endif
