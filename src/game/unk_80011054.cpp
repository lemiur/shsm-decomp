// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80011054.h>
#include <string.h>

// Six names, each padded to a word: "Expression", "Expression", "TransitionType", "TimedBlend",
// "TransitionType", "Expression". Only the first is compared, by the tail of each entry name.
char lbl_803DFFA0[0x50] = "Expression\0\0Expression\0\0TransitionType\0\0TimedBlend\0\0TransitionType\0\0Expression";

/*
====================
fn_80011054
====================
*/
// 0x80011054
int fn_80011054(unk_ExpressionList* list, void* unused, void* out) {
    int matched = 0;
    for (int i = 0; i < list->count; i++) {
        unk_ExpressionEntry* entry = fn_80188208(list, i);
        size_t len = strlen(entry->name);
        if (strcmp(entry->name + len - 10, lbl_803DFFA0) == 0) {
            matched++;
            fn_801B651C(out, fn_80189D10(entry, "ExpName")->text + 4);
        }
    }
    return 0x2c + matched * 16;
}
