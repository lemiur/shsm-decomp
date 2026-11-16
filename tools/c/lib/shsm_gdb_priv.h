/* Helpers shared by shsm_gdb.c and shsm_gdb_broker.c (not part of the module's API). */
#pragma once
#include "shsm_gdb.h"

AvpStr gdbp_latin1_to_utf8(AvpArena *a, AvpStr s);       /* each byte -> its code point */
AvpStr gdbp_utf8_to_latin1(AvpArena *a, AvpStr s);       /* code points > 0xFF become '?' */
const char *gdbp_cz_latin1(AvpArena *a, AvpStr s);       /* Latin-1 bytes -> NUL-terminated UTF-8 */
bool gdbp_int16(AvpStr s, u64 *out);                     /* int(s, 16) for non-negative values below 2**64 */
u64 gdbp_int16_or_raise(AvpArena *a, AvpStr s);          /* ... else ValueError "invalid literal for int() ..." */
ShsmBytes gdbp_fromhex(AvpArena *a, AvpStr s);           /* bytes.fromhex(s), Python's ValueError texts */
AvpStr gdbp_hex(AvpArena *a, const void *p, size_t n);   /* bytes.hex() */
const char *gdbp_type_name(const AvpJson *v);            /* type(json.loads value).__name__ */
