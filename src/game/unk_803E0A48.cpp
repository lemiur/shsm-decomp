// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_803E0A48.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data size 0xC __vt__12unk_803E0A48

/*
====================
~unk_803E0A48
====================
*/
// 0x80017B10
unk_803E0A48::~unk_803E0A48() {
    if (name != NULL) {
        delete name;
        name = NULL;
    }
}
