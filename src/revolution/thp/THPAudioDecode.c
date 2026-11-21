// unit name unconfirmed: THPAudioDecode.c is the object name in the Gormiti (Debug) link map, not a string
// in this DOL, and this range does not pair with that object by body; rename it when evidence appears
// and keep the open item in docs/exceptions.md.

#include <revolution/thp/__thp.h>

/*
====================
fn_80099C64
====================
*/
// 0x80099C64
void fn_80099C64(void) {
    if (lbl_80529770 != 0) {
        OSCancelThread(&lbl_8046A7B0.thread);
        lbl_80529770 = 0;
    }
}

/*
====================
AudioDecoder
====================
*/
// 0x80099CA0
void* AudioDecoder(void* arg) {
    void* buffer;

    for (;;) {
        buffer = fn_8009C1A4();
        AudioDecode(buffer);
        fn_8009C220(buffer);
    }
}
