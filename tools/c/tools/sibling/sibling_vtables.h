/* sibling: the sibling ELF's symbol table and vtable objects, and the stored pairing as a lookup by retail address,
 * for tools that build class declarations (classes.exe decl).
 *
 * The sibling (Gormiti) keeps its .symtab: every `__vt__<class>` object is the vtable of exactly that class, and its
 * words (read from the ELF's data sections; an executable ELF holds final addresses, no relocations) are the ordered
 * slot functions: two header words (RTTI pointer and offset, 0 with -RTTI off), then one function address per slot,
 * 0 for a pure virtual. Read on demand from the ELF the index recorded (meta 'elf'); nothing is stored. Evidence only. */
#pragma once
#include "sibling.h"

typedef struct {
    const char *name;
    u32 address, size;
    u8 bind, type; /* STB_* (0 local, 1 global, 2 weak), STT_* (1 object, 2 function) */
} SibElfSym;

typedef struct {
    const char *symbol;  /* __vt__Q24FMOD4DSPI */
    const char *cls;     /* the demangled class, FMOD::DSPI */
    u32 address, size;
    bool weak;
    const char **slots;  /* the function each slot word names; NULL for a 0 word ("" for an address no symbol names) */
    u32 *slot_addr;
    size_t n;            /* slots: (size - 8) / 4 */
} SibVtable;

typedef struct {
    SibElfSym *syms;
    size_t nsyms;
    SibVtable *vt;
    size_t nvt;
    AvpMap *by_name;     /* symbol name -> index + 1 (the first global or weak definition wins over a local) */
    AvpIMap *fn_at;      /* function address -> index + 1 (a global or weak function preferred) */
} SibElf;

/* Parse a big-endian ELF32's .symtab and read every __vt__ object; NULL when the bytes are not such an ELF. */
SibElf *sibling_elf_parse(AvpArena *a, AvpStr elf);
/* The ELF the index recorded (meta 'elf'), else orig/_ref/gormiti_wii/Exe/GormitiFinal.elf under the root. */
const char *sibling_elf_path(AvpArena *a, Sibling *sb);
const SibElfSym *sibling_elf_sym(const SibElf *e, const char *name); /* NULL when absent */
const char *sibling_elf_fn_at(const SibElf *e, u32 address);       /* NULL when no function starts there */
const SibVtable *sibling_elf_vtable(const SibElf *e, const char *cls_or_symbol); /* by class or symbol */

/* One retail function's counterpart in the stored pairing. */
typedef struct {
    const char *name; /* the sibling function's (mangled) name */
    u32 address;      /* its sibling address */
    const char *tier;
} SibPeer;
/* retail address -> SibPeer* for every paired retail function; an empty map without pairs. */
AvpIMap *sibling_peer_map(AvpArena *a, Sibling *sb);
