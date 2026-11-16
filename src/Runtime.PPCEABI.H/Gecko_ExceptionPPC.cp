#include <PowerPC_EABI_Support/Runtime/__ppc_eabi_linker.h>
#include <PowerPC_EABI_Support/Runtime/NMWException.h>

#define MAX_FRAGMENTS 32

/* One registered code fragment: its exception table index and TOC. */
typedef struct ExceptionInfo {
    __eti_init_info* exception_record;
    char* TOC;
    int active;
} ExceptionInfo;

static ExceptionInfo fragmentinfo[MAX_FRAGMENTS];

/*
 * __register_fragment
 *
 * Enters a code fragment in the first free slot of the fragment table and
 * returns its ID, or -1 when the table is full.
 */
int __register_fragment(__eti_init_info* info, char* TOC) {
    ExceptionInfo* fragment;
    int i;

    for (i = 0, fragment = fragmentinfo; i < MAX_FRAGMENTS; i++, fragment++) {
        if (fragment->active == 0) {
            fragment->exception_record = info;
            fragment->TOC = TOC;
            fragment->active = 1;
            return i;
        }
    }
    return -1;
} // 0x801A7044

/*
 * __unregister_fragment
 *
 * Frees the table slot of a registered code fragment.
 */
void __unregister_fragment(int fragmentID) {
    ExceptionInfo* fragment;

    if ((unsigned)fragmentID <= MAX_FRAGMENTS - 1) {
        fragment = &fragmentinfo[fragmentID];
        fragment->exception_record = 0;
        fragment->TOC = 0;
        fragment->active = 0;
    }
} // 0x801A7090
