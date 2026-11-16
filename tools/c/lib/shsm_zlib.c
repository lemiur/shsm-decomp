/* zlib wrappers (zlib-ng in zlib-compatible mode): compress / decompress and the streaming objects, written to give
 * the bytes and the error texts of CPython 3.14's zlib module. See shsm_zlib.h. Taken from another project of the author's tools/c/core/src/zip.c at commit 49e811503d7ba5c9cff92160557df057c50a8ab3 (names avp_ -> shsm_). */
#include "shsm_zlib.h"
#include "avp/arena.h"

#include <stdio.h>
#include <zlib.h>

/* ---- CPython's output buffer: the sizes of the avail_out windows deflate is called with ----------------------------
 * CPython 3.14 (Modules/zlibmodule.c, its output buffer) hands deflate a first window of 32 KiB, then 64 KiB,
 * 256 KiB, 1 MiB, 4 MiB ... (found by decoding the block boundaries of its level 0 output), and a compressor can emit different bytes depending on how much room the current call
 * has (the stored blocks of level 0 are cut at the window; level 1's quick strategy flushes at it), so the compressor
 * here is driven with the same windows to give the same bytes. The window sizes only matter for the bytes
 * deflate() produces, not for where the bytes are stored (one contiguous vector here). */
static const size_t k_windows[] = {
    32 * 1024, 64 * 1024, 256 * 1024, 1024 * 1024, 4 * 1024 * 1024, 8 * 1024 * 1024, 16 * 1024 * 1024,
    16 * 1024 * 1024, 32 * 1024 * 1024, 32 * 1024 * 1024, 32 * 1024 * 1024, 32 * 1024 * 1024, 64 * 1024 * 1024,
    64 * 1024 * 1024, 128 * 1024 * 1024, 256 * 1024 * 1024};

static size_t window_size(size_t index) {
    return k_windows[index < AVP_ARRAY_LEN(k_windows) ? index : AVP_ARRAY_LEN(k_windows) - 1];
}

/* CPython's zlib_error(): "Error <code> <what>: <msg>" with the stream's message, or a standard text for the three
 * codes that carry none. */
static void set_err(ShsmZErr *err, int code, const char *zmsg, const char *what) {
    if (!err) return;
    const char *msg = zmsg;
    if (code == Z_VERSION_ERROR) msg = "library version mismatch";
    if (!msg) {
        switch (code) {
        case Z_BUF_ERROR: msg = "incomplete or truncated stream"; break;
        case Z_STREAM_ERROR: msg = "inconsistent stream state"; break;
        case Z_DATA_ERROR: msg = "invalid input data"; break;
        default: break;
        }
    }
    err->code = code;
    if (msg) snprintf(err->msg, sizeof err->msg, "Error %d %s: %.200s", code, what, msg);
    else snprintf(err->msg, sizeof err->msg, "Error %d %s", code, what);
}

/* ---- compression --------------------------------------------------------------------------------------------------- */

struct ShsmZComp {
    z_stream zs;
};

ShsmZComp *shsm_zcomp_new(int level, int wbits) {
    ShsmZComp *c = (ShsmZComp *)avp_xcalloc(1, sizeof *c);
    /* the same parameters as zlib.compress / compressobj: DEFLATED, memLevel DEF_MEM_LEVEL (8), default strategy */
    if (deflateInit2(&c->zs, level, Z_DEFLATED, wbits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        avp_xfree(c);
        return NULL;
    }
    return c;
}

/* Runs deflate with `flush` over the input already set in zs, in CPython's windows, appending to out. */
static bool deflate_run(z_stream *zs, int flush, AvpBytes *out) {
    size_t window = 0;
    int err;
    do {
        size_t room = window_size(window++);
        size_t base = out->n;
        avp_vec_reserve(out, base + room);
        zs->next_out = out->v + base;
        zs->avail_out = (uInt)room;
        err = deflate(zs, flush);
        out->n = base + (room - zs->avail_out);
        if (err == Z_STREAM_ERROR) return false;
    } while (zs->avail_out == 0);
    return true;
}

bool shsm_zcomp_compress(ShsmZComp *c, const void *p, size_t n, AvpBytes *out) {
    const u8 *q = (const u8 *)p;
    while (n) {
        size_t k = n > 0xFFFFFFFFu ? 0xFFFFFFFFu : n;
        c->zs.next_in = (Bytef *)q;
        c->zs.avail_in = (uInt)k;
        /* like CPython, keep calling until this piece of input is used up */
        do {
            if (!deflate_run(&c->zs, Z_NO_FLUSH, out)) return false;
        } while (c->zs.avail_in != 0);
        q += k;
        n -= k;
    }
    return true;
}

bool shsm_zcomp_flush(ShsmZComp *c, int mode, AvpBytes *out) {
    if (mode == Z_NO_FLUSH) return true; /* Python: flush(Z_NO_FLUSH) returns b"" */
    c->zs.next_in = NULL;
    c->zs.avail_in = 0;
    return deflate_run(&c->zs, mode, out);
}

void shsm_zcomp_free(ShsmZComp *c) {
    if (!c) return;
    deflateEnd(&c->zs);
    avp_xfree(c);
}

u8 *shsm_zlib_compress(AvpArena *a, const void *p, size_t n, int level, int wbits, size_t *out_n) {
    ShsmZComp *c = shsm_zcomp_new(level, wbits);
    if (!c) return NULL;
    AvpBytes out = {0};
    /* zlib.compress makes ONE deflate(Z_FINISH) call over the whole input (compressobj.compress uses Z_NO_FLUSH and a
     * later flush, which gives different bytes at levels 0 and 1); the input is passed whole up to 4 GiB - 1 */
    AVP_ASSERT(n <= 0xFFFFFFFFu);
    c->zs.next_in = (Bytef *)p;
    c->zs.avail_in = (uInt)n;
    bool ok = deflate_run(&c->zs, Z_FINISH, &out);
    shsm_zcomp_free(c);
    if (!ok) {
        avp_vec_free(&out);
        return NULL;
    }
    u8 *r = (u8 *)avp_memdup(a, out.v ? out.v : (u8 *)"", out.n);
    *out_n = out.n;
    avp_vec_free(&out);
    return r;
}

/* ---- decompression -------------------------------------------------------------------------------------------------- */

struct ShsmZDecomp {
    z_stream zs;
    bool eof;
    AvpBytes tail;   /* unconsumed_tail */
    AvpBytes unused; /* unused_data */
};

ShsmZDecomp *shsm_zdecomp_new(int wbits) {
    ShsmZDecomp *d = (ShsmZDecomp *)avp_xcalloc(1, sizeof *d);
    if (inflateInit2(&d->zs, wbits) != Z_OK) {
        avp_xfree(d);
        return NULL;
    }
    return d;
}

void shsm_zdecomp_free(ShsmZDecomp *d) {
    if (!d) return;
    inflateEnd(&d->zs);
    avp_vec_free(&d->tail);
    avp_vec_free(&d->unused);
    avp_xfree(d);
}

bool shsm_zdecomp_decompress(ShsmZDecomp *d, const void *p, size_t n, size_t max_length, AvpBytes *out, ShsmZErr *err) {
    if (err) {
        err->code = 0;
        err->msg[0] = 0;
    }
    AVP_ASSERT(n <= 0xFFFFFFFFu); /* the tools feed chunks of a few MiB; CPython's >4 GiB piecewise feeding is not needed */
    if (d->eof) {
        /* after the end of the stream everything is unused data, as Python's */
        avp_vec_extend(&d->unused, (const u8 *)p, n);
        return true;
    }
    size_t produced = 0;
    int zerr = Z_OK;
    avp_vec_clear(&d->tail);
    d->zs.next_in = (Bytef *)p;
    d->zs.avail_in = (uInt)n;
    do {
        size_t room = 16384; /* DEF_BUF_SIZE; the window only bounds one inflate call */
        if (max_length) {
            if (produced >= max_length) break;
            if (room > max_length - produced) room = max_length - produced;
        }
        size_t base = out->n;
        avp_vec_reserve(out, base + room);
        d->zs.next_out = out->v + base;
        d->zs.avail_out = (uInt)room;
        zerr = inflate(&d->zs, Z_SYNC_FLUSH);
        size_t got = room - d->zs.avail_out;
        out->n = base + got;
        produced += got;
        if (zerr != Z_OK && zerr != Z_BUF_ERROR && zerr != Z_STREAM_END) {
            set_err(err, zerr, d->zs.msg, "while decompressing data");
            return false;
        }
    } while (d->zs.avail_out == 0);
    /* save_unconsumed_input: the rest of the input is unused data after the end of the stream, else the tail */
    if (zerr == Z_STREAM_END) {
        d->eof = true;
        if (d->zs.avail_in) avp_vec_extend(&d->unused, d->zs.next_in, d->zs.avail_in);
    } else if (d->zs.avail_in) {
        avp_vec_extend(&d->tail, d->zs.next_in, d->zs.avail_in);
    }
    return true;
}

const u8 *shsm_zdecomp_unconsumed_tail(const ShsmZDecomp *d, size_t *n) {
    *n = d->tail.n;
    return d->tail.v;
}
bool shsm_zdecomp_eof(const ShsmZDecomp *d) { return d->eof; }
const u8 *shsm_zdecomp_unused_data(const ShsmZDecomp *d, size_t *n) {
    *n = d->unused.n;
    return d->unused.v;
}

u8 *shsm_zlib_decompress(AvpArena *a, const void *p, size_t n, int wbits, size_t size_hint, size_t *out_n, ShsmZErr *err) {
    if (err) {
        err->code = 0;
        err->msg[0] = 0;
    }
    z_stream zs;
    memset(&zs, 0, sizeof zs);
    int zerr = inflateInit2(&zs, wbits);
    if (zerr != Z_OK) {
        set_err(err, zerr, zs.msg, "while preparing to decompress data");
        return NULL;
    }
    AvpBytes out = {0};
    size_t cap = size_hint ? size_hint + 1 : (n < 4096 ? 16384 : n * 3);
    avp_vec_reserve(&out, cap);
    AVP_ASSERT(n <= 0xFFFFFFFFu);
    zs.next_in = (Bytef *)p;
    zs.avail_in = (uInt)n;
    do {
        if (out.cap - out.n < 4096) avp_vec_reserve(&out, out.cap * 2);
        size_t room = out.cap - out.n;
        if (room > 0xFFFFFFFFu) room = 0xFFFFFFFFu;
        zs.next_out = out.v + out.n;
        zs.avail_out = (uInt)room;
        zerr = inflate(&zs, Z_SYNC_FLUSH);
        out.n += room - zs.avail_out;
        if (zerr != Z_OK && zerr != Z_BUF_ERROR && zerr != Z_STREAM_END) {
            set_err(err, zerr, zs.msg, "while decompressing data");
            inflateEnd(&zs);
            avp_vec_free(&out);
            return NULL;
        }
    } while (zs.avail_out == 0);
    if (zerr != Z_STREAM_END) {
        /* input ran out before the end of the stream: CPython 3.14 reports Z_BUF_ERROR, not Z_OK */
        set_err(err, zerr == Z_OK ? Z_BUF_ERROR : zerr, zs.msg, "while decompressing data");
        inflateEnd(&zs);
        avp_vec_free(&out);
        return NULL;
    }
    inflateEnd(&zs);
    u8 *r = (u8 *)avp_memdup(a, out.v, out.n);
    *out_n = out.n;
    avp_vec_free(&out);
    return r;
}

/* ---- checksums -------------------------------------------------------------------------------------------------- */

/* zlib's crc32/adler32 take a uInt length: long inputs are fed in pieces, as CPython does. */
u32 shsm_zlib_crc32(const void *p, size_t n, u32 value) {
    const u8 *b = (const u8 *)p;
    uLong v = value;
    while (n) {
        uInt k = n > 0x40000000u ? 0x40000000u : (uInt)n;
        v = crc32(v, b, k);
        b += k;
        n -= k;
    }
    return (u32)v;
}

u32 shsm_zlib_adler32(const void *p, size_t n, u32 value) {
    const u8 *b = (const u8 *)p;
    uLong v = value;
    while (n) {
        uInt k = n > 0x40000000u ? 0x40000000u : (uInt)n;
        v = adler32(v, b, k);
        b += k;
        n -= k;
    }
    return (u32)v;
}
