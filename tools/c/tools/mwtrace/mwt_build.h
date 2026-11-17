/* The compiler builds mwtrace knows: per build, the SHA-1 of tools/compilers/<build>/mwcceppc.exe and the addresses
 * the traces break at. The two builds the project uses (Wii/1.0, Wii/1.0a) are different compiles of the same
 * source (registers and some data layouts differ), so every address was read from that build's own disassembly
 * (pe_disasm.exe) and is used only on an exe with that hash. docs/compiler_inline.md lists what each one is. */
#pragma once
#include "avp/avp.h"

typedef struct {
    const char *key; /* the {KEY} the spec templates use */
    u32 addr;
} MwtAddr;

typedef struct {
    const char *build; /* "Wii/1.0" */
    const char *sha1;  /* of tools/compilers/<build>/mwcceppc.exe */
    const MwtAddr *addrs;
    size_t naddrs;
} MwtBuild;

/* The table of build, or NULL when mwtrace has none for it. */
const MwtBuild *mwt_build_find(const char *build);
/* The value of {key} in b; false when the table has no such key. */
bool mwt_build_value(const MwtBuild *b, const char *key, u32 *out);
/* Checks that root/tools/compilers/<build>/mwcceppc.exe has the table's hash: false with *why otherwise. */
bool mwt_build_verify(AvpArena *a, const char *root, const MwtBuild *b, const char **why);
/* The template with every {KEY} replaced by the table's value (hex, "0x..."); false with *why naming the first key
 * the table does not have. */
bool mwt_build_expand(AvpArena *a, const MwtBuild *b, AvpStr tmpl, AvpStr *out, const char **why);
