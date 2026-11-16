/* sibling_mdebug: a 32-bit little-endian MIPS ELF (the PS2 builds of the Climax lineage: Ghost Rider) read for
 * its section headers, its .symtab and its .mdebug section: the ECOFF symbolic header (HDRR), file descriptors
 * (FDR), procedure descriptors (PDR), local symbols (SYMR), local strings and external symbols (EXTR). gcc writes its
 * stabs into the local symbols (index field 0x8F300 + stab code); sib_md_stabs collects them per file with the
 * backslash continuations joined. Everything points into the file image given, which must outlive the results.
 * Offsets in the HDRR are file offsets (ELF executables); offsets that fall outside the section are read as
 * section-relative. Bounds are checked: a damaged table reads as empty, never past the image. */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"
#include "avp/vec.h"

enum { SIB_EM_MIPS = 8 };

typedef struct {
    const char *name;
    u32 type, flags, addr, offset, size, link, entsize;
} SibElfSec;
typedef struct {
    const char *name;
    u32 value, size;
    u8 bind, type; /* STB_*, STT_* */
    u16 shndx;
} SibElfSym;
typedef struct {
    const u8 *data;
    size_t size;
    u16 machine;
    bool big_endian;
    AVP_VEC(SibElfSec) secs;
    AVP_VEC(SibElfSym) syms; /* .symtab, in table order */
} SibElf;
/* Parse an ELF32 image (either byte order); false with *err set when it is not one. */
bool sib_elf_parse(AvpArena *a, const u8 *data, size_t size, SibElf *out, const char **err);
const SibElfSec *sib_elf_section(const SibElf *e, const char *name);

/* ---- ECOFF symbolic information ---- */
enum { SIB_ST_NIL = 0, SIB_ST_GLOBAL = 1, SIB_ST_STATIC = 2, SIB_ST_PROC = 6, SIB_ST_END = 8, SIB_ST_FILE = 11,
       SIB_ST_STATICPROC = 14 };
#define SIB_STAB_MARK 0x8F300u /* ECOFF_IS_STAB: (index & 0xFFF00) == 0x8F300; the stab code is index - 0x8F300 */

typedef struct {
    i32 iss, value;
    u8 st, sc;
    u32 index;
} SibSymr;
typedef struct {
    u32 adr;
    const char *name; /* rss: the source path as compiled */
    i32 rss, issBase, cbSs, isymBase, csym, ipdFirst, cpd, iauxBase, caux;
    u8 lang;
} SibFdr;
typedef struct {
    u32 adr;   /* absolute: the procedure symbol's value, or the FDR address plus the PDR offset */
    i32 isym;  /* local symbol of the file */
    i32 fd;
    const char *name;
} SibPdr;
typedef struct {
    const char *name;
    i32 value;
    u8 st, sc;
    i16 ifd;
} SibExtr;
typedef struct {
    u16 magic, vstamp;
    bool be;
    i32 ilineMax, ipdMax, isymMax, ioptMax, iauxMax, issMax, issExtMax, ifdMax, crfd, iextMax;
    u32 cbSymOffset, cbSsOffset, cbSsExtOffset, cbFdOffset, cbPdOffset, cbExtOffset;
    const u8 *data; /* the whole image */
    size_t size;
    AVP_VEC(SibFdr) fds;
    AVP_VEC(SibPdr) pds;  /* in table order: file by file */
    AVP_VEC(SibExtr) exts;
    i64 stab_records;     /* filled by sib_md_stabs calls (diagnostics) */
} SibMdebug;
/* Parse the .mdebug section at [off, off+len) of the image. false with *err when the HDRR is not ECOFF (magic
 * 0x7009). */
bool sib_md_parse(AvpArena *a, const u8 *data, size_t size, u32 off, u32 len, SibMdebug *out, const char **err);
/* Local symbol i (0-based within the file) of file fd; false when out of bounds. */
bool sib_md_sym(const SibMdebug *m, i32 fd, i32 i, SibSymr *out);
/* A local string of file fd (NULL when out of bounds). Points into the image. */
const char *sib_md_lstr(const SibMdebug *m, i32 fd, i32 iss);

typedef struct {
    int type;      /* stab code: 0x24 N_FUN, 0x80 N_LSYM, 0x20 N_GSYM, 0x26 N_STSYM, 0x28 N_LCSYM, 0x64 N_SO, ... */
    i32 value;
    const char *str; /* joined across '\' continuations (arena) or pointing into the image */
} SibStab;
typedef AVP_VEC(SibStab) SibStabVec;
/* Append file fd's stab records to out, in symbol order. */
void sib_md_stabs(AvpArena *a, SibMdebug *m, i32 fd, SibStabVec *out);
