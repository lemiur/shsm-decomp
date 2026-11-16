/* The GameCube/Wii DOL executable: header (7 text + 11 data section offsets, addresses and sizes, bss, entry point),
 * address-to-file-offset mapping and reads at an address. Ports gatecheck.py's Dol/Segment and codecmp.py's Dol
 * (lib/shsm_dol.md). */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"

#define SHSM_DOL_SECTIONS 18 /* T0..T6, D0..D10 */
#define SHSM_DOL_TEXT_SECTIONS 7
#define SHSM_DOL_HEADER_SIZE 0x100

/* One non-empty section of the file (gatecheck.Segment). */
typedef struct ShsmDolSegment {
    int index;     /* 0..17: header slot (0..6 text, 7..17 data) */
    i64 off;       /* file offset */
    i64 addr;      /* load address */
    i64 size;      /* bytes */
    const u8 *data; /* size bytes (inside the DOL's buffer) */
} ShsmDolSegment;

typedef struct ShsmDol {
    const u8 *data; /* the whole file */
    size_t size;
    u32 offs[SHSM_DOL_SECTIONS], addrs[SHSM_DOL_SECTIONS], sizes[SHSM_DOL_SECTIONS];
    u32 bss_addr, bss_size, entry;
    ShsmDolSegment segs[SHSM_DOL_SECTIONS]; /* the non-empty sections in header order */
    size_t nsegs;
} ShsmDol;

/* Parses a DOL image (the bytes are referenced, not copied). Raises SHSM_EXC_VALUE with gatecheck's GateError text:
 * "DOL too short (N bytes)" below 0x100 bytes, "DOL section I runs past the end of the file". */
ShsmDol *shsm_dol_parse(AvpArena *a, const u8 *data, size_t size);
/* Dol.load: reads and parses path; an unreadable file raises SHSM_EXC_OS "cannot read <path>: <str(OSError)>". */
ShsmDol *shsm_dol_load(AvpArena *a, const char *path);

static inline i64 shsm_dol_seg_end(const ShsmDolSegment *s) { return s->addr + s->size; }
/* "T<i>" for text slots, "D<i-7>" for data slots (Segment.name), into buf (at least 4 bytes). */
const char *shsm_dol_seg_name(const ShsmDolSegment *s, char buf[4]);
static inline bool shsm_dol_seg_is_text(const ShsmDolSegment *s) { return s->index < SHSM_DOL_TEXT_SECTIONS; }

/* segment_at: the first segment (header order) holding addr, or NULL. */
const ShsmDolSegment *shsm_dol_segment_at(const ShsmDol *d, i64 addr);
/* The file offset of addr, or -1 when no segment holds it. */
i64 shsm_dol_file_offset(const ShsmDol *d, i64 addr);
/* gatecheck Dol.read: bytes [addr, addr+n) gathered across segments into out (allocated in a); false when any byte
 * lies outside every segment. n == 0 gives true and an empty string. */
bool shsm_dol_read(AvpArena *a, const ShsmDol *d, i64 addr, i64 n, AvpStr *out);
/* codecmp Dol.read: a view of [addr, addr+n) when it lies inside one segment (first match in header order), else
 * false. */
bool shsm_dol_read_within(const ShsmDol *d, i64 addr, i64 n, AvpStr *out);
/* bss_addr <= addr < bss_addr + bss_size. */
bool shsm_dol_in_bss(const ShsmDol *d, i64 addr);
