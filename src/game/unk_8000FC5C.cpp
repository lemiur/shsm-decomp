// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8000FC5C.h>
#include <string.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata 0x80526830 size 0x6 lbl_80526830
//   .sdata 0x80526838 size 0x6 lbl_80526838
//   .sdata 0x80526840 size 0x4 lbl_80526840
//   .sdata 0x80526844 size 0x5 lbl_80526844
//   .sdata 0x8052684C size 0x3 lbl_8052684C
//   .sdata 0x80526850 size 0x3 lbl_80526850
//   .sdata 0x80526854 size 0x6 lbl_80526854
//   .sdata 0x8052685C size 0x5 lbl_8052685C
//   .sdata 0x80526864 size 0x6 lbl_80526864
//   .sdata 0x8052686C size 0x6 lbl_8052686C
//   .sdata 0x80526874 size 0x7 lbl_80526874
//   .sdata 0x8052687C size 0x7 lbl_8052687C
//   .sdata 0x80526884 size 0x7 lbl_80526884
//   .sdata2 0x8052B758 size 0x4 lbl_8052B758
//   .sdata2 0x8052B75C size 0x4 lbl_8052B75C

/*
====================
fn_8000FC5C
====================
*/
// 0x8000FC5C
extern "C" void fn_8000FC5C() {
    // TODO
}

/*
====================
fn_800103EC
====================
*/
// Wall (register/schedule): the body matches the retail control flow and calls; retail keeps
// the count in r31, the index in r30 and the string base in r27, the compiled body numbers them
// differently (97.56%).
// 0x800103EC
extern "C" s32 fn_800103EC(unk_EventDescriptor* self, unk_EventChildren* children) {
    s32 i;
    s32 count = 0;
    for (i = 0; i < children->count; i++) {
        if (strcmp(fn_80189CCC(children, i)->node->name, "End Sub Sequence") == 0) {
            unk_EventChildren* subs = fn_80189CCC(children, i)->node->children;
            s32 subCount = 0;
            for (s32 j = 0; j < subs->count; j++) {
                if (strcmp(fn_80189CCC(subs, j)->node->name, "End Sub Sequence") == 0) {
                    unk_EventChildren* parts = fn_80189CCC(subs, j)->node->children;
                    s32 partCount = 0;
                    for (s32 k = 0; k < parts->count; k++) {
                        if (strcmp(fn_80189CCC(parts, k)->node->name, "End Sub Sequence") == 0) {
                            partCount += fn_800103EC(self, fn_80189CCC(parts, k)->node->children);
                        } else if (strcmp(fn_80189CCC(parts, k)->node->name, "Transition") == 0) {
                            partCount++;
                        }
                    }
                    subCount += partCount;
                } else if (strcmp(fn_80189CCC(subs, j)->node->name, "Transition") == 0) {
                    subCount++;
                }
            }
            count += subCount;
        } else if (strcmp(fn_80189CCC(children, i)->node->name, "Transition") == 0) {
            count++;
        }
    }
    return count;
}

/*
====================
fn_800105B8
====================
*/
// Wall (register/schedule): the body matches the retail control flow and calls; retail keeps the
// self, children and index parameters in r27-r29 with the subs and j locals in r26 and r25, the
// compiled body numbers them differently (94.23%).
// 0x800105B8
extern "C" s32 fn_800105B8(unk_EventDescriptor* self, unk_EventChildren* children, s32 index) {
    s32 i;
    for (i = 0; i < children->count; i++) {
        if (strcmp(fn_80189CCC(children, i)->node->name, "End Sub Sequence") == 0) {
            unk_EventChildren* subs = fn_80189CCC(children, i)->node->children;
            for (s32 j = 0; j < subs->count; j++) {
                if (strcmp(fn_80189CCC(subs, j)->node->name, "End Sub Sequence") == 0) {
                    index = fn_800105B8(self, fn_80189CCC(subs, j)->node->children, index);
                } else if (strcmp(fn_80189CCC(subs, j)->node->name, "Transition") == 0) {
                    unk_EventNode* node = fn_80189CCC(subs, j)->node;
                    self->entries[index++] = self->lookup->entries[node->valueKey->index].values[node->field_0x8];
                }
            }
        } else if (strcmp(fn_80189CCC(children, i)->node->name, "Transition") == 0) {
            unk_EventNode* node = fn_80189CCC(children, i)->node;
            self->entries[index++] = self->lookup->entries[node->valueKey->index].values[node->field_0x8];
        }
    }
    return index;
}

/*
====================
fn_80010784
====================
*/
// 0x80010784
extern "C" void fn_80010784() {
    // TODO
}

/*
====================
fn_80010B10
====================
*/
// 0x80010B10
extern "C" void fn_80010B10(unk_EventDescriptor* self, s32 value) {
    self->field_0x24 = value;
}

/*
====================
fn_80010B18
====================
*/
// 0x80010B18
extern "C" void* fn_80010B18(unk_EventDescriptor* self, s32 index) {
    if (self->child0 == NULL) {
        return NULL;
    }
    switch (index) {
    case 0:
        return self->child1;
    case 1:
        return self->child2;
    case 2:
        return self->child3;
    case 3:
        return self->child4;
    default:
        return NULL;
    }
}

/*
====================
fn_80010B78
====================
*/
// Wall (branch shape): retail branches on the bit-2 test to the bit-0 return (beq), the
// compiled form returns the 2 through bnelr and loads flags into another register.
// 0x80010B78
extern "C" s32 fn_80010B78(unk_EventDescriptor* self) {
    if (self->flags & 0x2) {
        return 2;
    } else {
        return self->flags & 0x1;
    }
}

/*
====================
fn_80010B94
====================
*/
// 0x80010B94
extern "C" void fn_80010B94() {
    // TODO
}

/*
====================
fn_80010E28
====================
*/
// 0x80010E28
extern "C" void fn_80010E28() {
}

/*
====================
fn_80010E2C
====================
*/
// 0x80010E2C
extern "C" void fn_80010E2C() {
    // TODO
}

/*
====================
fn_80010FC4
====================
*/
// 0x80010FC4
extern "C" void* fn_80010FC4(unk_EventDescriptor* self, void* key) {
    for (s32 i = 0; i < self->count; i++) {
        if (fn_800114CC(self->entries[i], key)) {
            return self->entries[i];
        }
    }
    return NULL;
}
