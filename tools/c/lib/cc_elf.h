/* The compiled object: a 32-bit big-endian relocatable ELF with RELA relocations (what mwcceppc writes), as
 * tools/codecmp.py's Elf class reads it. Everything is allocated from the arena given and stays valid with it.
 *
 *     CcElf *elf = cc_elf_parse(avp_arena(), data, n, &why);   // NULL and why = "not a 32-bit big-endian ELF object"
 *     const CcSym *f = cc_elf_function(elf, AVP_S("fn_800067CC__8unk_RampFf"));
 *     const CcReloc *r = cc_elf_reloc_at(elf, f->shndx, 0x10);
 *
 * Names are decoded as Latin-1 (the Python's .decode("latin-1")) and kept as UTF-8, so a byte 0xE9 becomes "é". */
#pragma once
#include "avp/avp.h"

enum { CC_SHT_PROGBITS = 1, CC_SHT_SYMTAB = 2, CC_SHT_RELA = 4, CC_SHT_NOBITS = 8 };
enum { CC_STT_OBJECT = 1, CC_STT_FUNC = 2, CC_STT_SECTION = 3 };
enum {
    CC_R_ADDR32 = 1, CC_R_ADDR16_LO = 4, CC_R_ADDR16_HI = 5, CC_R_ADDR16_HA = 6, CC_R_REL24 = 10, CC_R_REL14 = 11,
    CC_R_SDAREL16 = 32, CC_R_EMB_SDA2I16 = 107, CC_R_EMB_SDA2REL = 108, CC_R_EMB_SDA21 = 109
};

typedef struct {
    AvpStr name; /* NUL-terminated */
    u32 value, size;
    u8 bind, type;
    u16 shndx;
} CcSym;

typedef struct {
    u32 offset;
    u32 type; /* info & 0xFF */
    u32 sym;  /* info >> 8: index into CcElf.symbols */
    i32 addend;
} CcReloc;

typedef struct {
    AvpStr name;
    u32 type, flags, offset, size, link, info;
    AvpStr data; /* the section's bytes (empty for SHT_NOBITS; short when the file is) */
    /* The relocations of this section (from every .rela section whose info names it), one per offset (a later
     * entry for the same offset replaces an earlier one, as the Python dict does), sorted by offset. */
    const CcReloc *relocs;
    size_t nrelocs;
} CcSection;

typedef struct CcElf {
    CcSection *sections; /* index 0 is the null section */
    size_t nsections;
    CcSym *symbols;      /* every SHT_SYMTAB entry, in order (index 0 the null symbol) */
    size_t nsymbols;
    AvpMap *functions;   /* name -> (i64) index of the first STT_FUNC symbol of that name */
    AvpStr bytes;        /* the whole file */
} CcElf;

/* Elf(data): NULL with *why set ("not a 32-bit big-endian ELF object") for anything else. */
CcElf *cc_elf_parse(AvpArena *a, const void *data, size_t n, const char **why);
/* R_NAMES.get(type, "R_PPC_<n>"). */
const char *cc_reloc_type_name(AvpArena *a, u32 type);
/* section_name(shndx): "" for 0 and out-of-range indices. */
AvpStr cc_elf_section_name(const CcElf *e, u32 shndx);
/* The relocation at exactly this offset of section shndx, or NULL. */
const CcReloc *cc_elf_reloc_exact(const CcElf *e, u32 shndx, i64 offset);
/* reloc_at(shndx, offset): the relocation of the instruction word at offset (or of its 16-bit half at offset+2). */
const CcReloc *cc_elf_reloc_at(const CcElf *e, u32 shndx, i64 offset);
/* functions.get(name): the first STT_FUNC symbol of that name, or NULL. */
const CcSym *cc_elf_function(const CcElf *e, AvpStr name);
/* find_symbol(name, shndx): the first symbol of that name (in section shndx when shndx >= 0), or NULL. */
const CcSym *cc_elf_find_symbol(const CcElf *e, AvpStr name, i64 shndx);
