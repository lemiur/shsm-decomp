// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8007BAD4.h>

/*
====================
unk_8007BAD4::unk_8007BAD4
====================
*/
// 0x8007BAD4
unk_8007BAD4::unk_8007BAD4() {
    record = NULL;
    field_0xC = 0.0f;
    field_0x8 = 0.0f;
    field_0x4 = 0.0f;
    field_0x10 = 0;
}

/*
====================
unk_8007BAD4::~unk_8007BAD4
====================
*/
// 0x8007BAF4
unk_8007BAD4::~unk_8007BAD4() {
    if (record != NULL) {
        delete record;
        record = NULL;
    }
}
