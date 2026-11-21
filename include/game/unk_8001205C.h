#ifndef GAME_UNK_8001205C_H
#define GAME_UNK_8001205C_H

// unit name unconfirmed: no string or class-name evidence names this unit; the header is named
// after the address of the unit's first function.

#include <revolution/types.h>

class unk_StartNode;
class unk_StartNodeGroup;
class unk_NodeSet;
class unk_NodeOwner;
class unk_NodeChild;
class unk_NodeEntry;
class unk_NodeLink;
class unk_NodeProps;

/*
 * A node of the start-node graph; its set (+0x20) carries the index of the group's record row.
 * Name invented.
 */
class unk_StartNode {
public:
    u8              field_0x0[0x8];
    s16             index;          // +0x8, the node's column in a record
    u8              field_0xA[0x16];
    unk_NodeSet*    set;            // +0x20
};

/*
 * The set a node belongs to (name invented); its index selects the group's record row.
 */
class unk_NodeSet {
public:
    u8              field_0x0[0xE];
    s16             index;          // +0xE
};

/*
 * One row of a group's record table, 8 bytes per row (name invented). The items are indexed by
 * the node's index.
 */
struct unk_StartNodeRecord {
    u8              field_0x0[4];
    void**          items;          // +0x4
};

/*
 * The group that files start nodes into two lists of entries and into a record table (name
 * invented). A list that is not allocated (NULL) only counts its entries.
 */
class unk_StartNodeGroup {
public:
    void            fn_8001205C( unk_StartNode* node );             // 0x8001205C
    void            fn_800121A0( unk_StartNode* node, int arg );    // 0x800121A0
    void            fn_800122A4();                                  // 0x800122A4

    unk_StartNodeRecord* records;       // +0x0
    unk_NodeEntry** entries;            // +0x4, 0x68-byte entries
    unk_NodeLink**  links;              // +0x8, 0x2C-byte links
    int*            startIndices;       // +0xC, indexed by the set's index
    int             nodeCount;          // +0x10
    int             field_0x14;
    int             unallocatedA;       // +0x18, entries counted while entries is NULL
    int             countA;             // +0x1C
    int             unallocatedB;       // +0x20, links counted while links is NULL
    int             countB;             // +0x24
    u8              field_0x28[0x10];
    int             total;              // +0x38
};

/*
 * An entry of the group's first list (0x68 bytes). Its constructor is fn_8000FC5C; the layout
 * is not yet known.
 */
class unk_NodeEntry {
public:
    u8              field_0x0[0x68];
};

/*
 * A link of the group's second list (0x2C bytes). Its constructor is fn_800110FC; the layout is
 * not yet known.
 */
class unk_NodeLink {
public:
    u8              field_0x0[0x2C];
};

/*
 * A child of the owner, 0x3C bytes, with a reference count (name invented). Its constructor is
 * fn_80011BB0.
 */
class unk_NodeChild {
public:
    u8              field_0x0[0x30];
    int             refCount;       // +0x30
    u8              field_0x34[0x8];
};

/*
 * The value of a node property: its isStartNode flag at +0x4 (the flag takes its name from the
 * property string).
 */
class unk_NodePropValue {
public:
    u8              field_0x0[0x4];
    s8              isStartNode;    // +0x4
};

/*
 * A property slot: its first word points to the value (name invented).
 */
struct unk_NodePropSlot {
    unk_NodePropValue* value;       // +0x0
};

/*
 * Callees in units not yet reconstructed: the node property lookups, the constructors of the
 * group's entries and links, and the owner's child query.
 */
extern "C" {
    int                 fn_8000F804( unk_StartNode* node );
    int                 fn_8000F8F8( unk_StartNode* node, unk_StartNodeGroup* group );
    unk_NodeEntry*      fn_8000FC5C( unk_NodeEntry* entry, unk_StartNode* node, int index, unk_StartNodeGroup* group );
    int                 fn_80011054( unk_StartNode* node, unk_StartNodeGroup* group, int arg );
    unk_NodeLink*       fn_800110FC( unk_NodeLink* link, unk_StartNode* node, int index, unk_StartNodeGroup* group, int arg );
    unk_NodeChild*      fn_80011BB0( unk_NodeChild* child, unk_NodeOwner* owner );
    unk_NodeChild*      fn_801874FC( unk_NodeOwner* owner );
    unk_NodeProps*      fn_80188218( unk_StartNode* node, const char* name );
    unk_NodePropSlot*   fn_80189D10( unk_NodeProps* props, const char* name );
}

/*
 * Returns the owner's child, or creates one and takes a reference to it (name invented; the
 * owner class is not yet reconstructed).
 */
extern "C" unk_NodeChild* fn_800122C0( unk_NodeOwner* owner );  // 0x800122C0

#endif
