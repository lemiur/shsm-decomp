/* zlib for the tools (replaces Python's zlib module as the tools use it: objcache.py's zlib.compress / decompress of
 * its cache headers and manifests, eqcheck.py's zlib.crc32, hashes.py's binascii.crc32). Taken from another project of the author's
 * tools/c/core/include/avp/zip.h (its zlib part) at commit 49e811503d7ba5c9cff92160557df057c50a8ab3, names avp_ -> shsm_; crc32 and adler32 added.
 *
 * zlib is zlib-ng 2.2.4 in zlib-compatible mode, the library CPython 3.14 links, so `zlib.compress(data, level)`
 * and shsm_zlib_compress(.., level) give the same bytes for every level (shsm_zlib_test.c compares a corpus at levels -1,
 * 0, 1, 6 and 9; the compressor is run the way CPython runs it: deflateInit2 with wbits 15, memLevel 8, default
 * strategy, one deflate(Z_FINISH) call over the whole input).
 *
 * Errors: functions return false / NULL and, where an `err` out-parameter exists, fill it with the text Python's
 * exception would carry (zlib.error: "Error -3 while decompressing data: incorrect header check"). `err` may be
 * NULL. Memory comes from the arena the caller gives unless a function says "heap" (AvpBytes grown by the caller's
 * vector, freed with avp_vec_free). Nothing here keeps global state: all functions are thread-safe on separate
 * objects. */
#pragma once
#include "avp/str.h"
#include "avp/vec.h"

/* ================================================================================================================
 * zlib
 * ================================================================================================================ */

/* Where the text of an error goes (zlib.error's str()). */
typedef struct {
    int code;      /* the zlib return code (Z_DATA_ERROR = -3, Z_BUF_ERROR = -5, ...), 0 when none */
    char msg[96];
} ShsmZErr;

/* zlib.compress(data, level, wbits=15): level -1 (the default, 6) .. 9; wbits 15 is the zlib wrapper, -15 raw
 * deflate, 31 gzip. The result is in the arena; *out_n is its length. NULL only for an invalid level / wbits. */
u8 *shsm_zlib_compress(AvpArena *a, const void *p, size_t n, int level, int wbits, size_t *out_n);
/* zlib.decompress(data, wbits=15). Like Python, input after the end of the stream is ignored; a stream that ends
 * early is an error ("Error -5 while decompressing data: incomplete or truncated stream"). size_hint is the
 * expected output size (0: guess); a wrong hint only costs a regrow. */
u8 *shsm_zlib_decompress(AvpArena *a, const void *p, size_t n, int wbits, size_t size_hint, size_t *out_n, ShsmZErr *err);

/* zlib.compressobj(level, DEFLATED, wbits).compress / .flush. Output is appended to a heap vector. */
typedef struct ShsmZComp ShsmZComp;
enum { AVP_Z_NO_FLUSH = 0, AVP_Z_SYNC_FLUSH = 2, AVP_Z_FULL_FLUSH = 3, AVP_Z_FINISH = 4 };
ShsmZComp *shsm_zcomp_new(int level, int wbits); /* NULL for an invalid level / wbits */
bool shsm_zcomp_compress(ShsmZComp *c, const void *p, size_t n, AvpBytes *out);
bool shsm_zcomp_flush(ShsmZComp *c, int mode, AvpBytes *out); /* Z_FINISH ends the stream */
void shsm_zcomp_free(ShsmZComp *c);

/* zlib.decompressobj(wbits): decompress(data, max_length), .unconsumed_tail, .eof, .unused_data. */
typedef struct ShsmZDecomp ShsmZDecomp;
ShsmZDecomp *shsm_zdecomp_new(int wbits);
/* Appends the output to out. max_length 0 means unlimited; otherwise at most that many bytes are produced and the
 * input not yet used is kept as the unconsumed tail (the caller feeds it again with the next chunk, as with
 * Python). False on a data error (err filled). */
bool shsm_zdecomp_decompress(ShsmZDecomp *d, const void *p, size_t n, size_t max_length, AvpBytes *out, ShsmZErr *err);
const u8 *shsm_zdecomp_unconsumed_tail(const ShsmZDecomp *d, size_t *n); /* valid until the next call */
bool shsm_zdecomp_eof(const ShsmZDecomp *d);                              /* the end of the stream was seen */
const u8 *shsm_zdecomp_unused_data(const ShsmZDecomp *d, size_t *n);     /* bytes after the end of the stream */
void shsm_zdecomp_free(ShsmZDecomp *d);


/* zlib.crc32(data, value=0) and binascii.crc32(data, value=0): the running CRC-32 (unsigned, as Python 3 returns it);
 * pass the previous result as value to continue. zlib.adler32(data, value=1) likewise. */
u32 shsm_zlib_crc32(const void *p, size_t n, u32 value);
u32 shsm_zlib_adler32(const void *p, size_t n, u32 value);
