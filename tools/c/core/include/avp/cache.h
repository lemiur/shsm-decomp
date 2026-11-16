/* The binary cache format that replaces pickle (design.md section 8): objcache's PickleMemo tables, the gate
 * snapshots and lane records, the annotation store, proto_pair / console_map / server_map results. Pickle files were
 * read with `except Exception: start afresh`; this format keeps that contract and makes it checkable: a cache file
 * that is missing, torn by a crash, zero-filled, written by another version, or made from other inputs is "no
 * cache" (the open function returns false) and is rebuilt by the tool. Never a crash, never a partial read.
 *
 * ---- Format 1 (all integers little-endian, the same on every machine) ----------------------------------------------
 *
 *   offset  size  field
 *        0     4  magic "AVPC"                  (a log file has "AVPL", see below)
 *        4     4  u32 format = 1                this container; any other value is "no cache"
 *        8     4  u32 schema                    the caller's version of the payload layout (bump it when a record
 *                                               changes); must equal what the reader expects
 *       12     4  u32 flags = 0                 reserved
 *       16     4  u32 kind_len                  <= 255
 *       20     4  u32 stamp_len                 <= 1 MiB
 *       24     8  u64 payload_len
 *       32     8  u64 payload_hash              avp_hash64(payload, payload_len, AVP_CACHE_SEED)
 *       40     4  u32 header_crc                crc32 of bytes 0..39 followed by the kind and stamp bytes
 *       44     4  u32 reserved = 0
 *       48  kind_len   kind                     "tool/kind" text, e.g. "gate/snapshot": a file of another kind is
 *                                               refused
 *        .. stamp_len  stamp                    the input stamp the caller supplies (a sha256, a list of hashes, a
 *                                               commit id...): the cache is valid only for exactly these bytes
 *        ..  0..7     zero padding up to a multiple of 8 from the file start (must be zero: every byte of a file is
 *                     covered by a check, so no single altered byte is accepted)
 *        P  payload_len  payload                typed records, below
 *   The file length must be exactly P + payload_len: a truncated file and a file with a tail are both refused.
 *
 * ---- Payload: typed, length-prefixed records -----------------------------------------------------------------------
 *   u8 u16 u32 u64 i32 i64    fixed width, little-endian (bool is a u8 0 or 1)
 *   f64                       the 8 bytes of the IEEE double (bit exact, NaN payloads and -0.0 survive)
 *   bytes / str               u32 length, then that many bytes (str is UTF-8 by convention, no NUL needed)
 *   record                    u32 byte length, then the fields of the record. A reader that finds fewer fields than
 *                             a newer writer put in skips to the record's end on its own (forward compatible), and
 *                             one that wants more fields than there are gets sticky "bad" (old file: no cache).
 *   vector of records         u32 count, then count records;  map: a vector of records (key, value, ...)
 * The reader never reads outside the mapped file: every read is bounds-checked against the record it is in, a read
 * past the end sets the reader's sticky `bad` flag and returns 0 / empty; counts are checked against the bytes that
 * remain (avp_cr_count) so a damaged count cannot make a caller allocate gigabytes. Check avp_cr_ok() once, after
 * the whole payload has been read, and treat bad as "no cache". (The payload checksum already rejects damaged
 * files; the bounds checks make a bug in a record layout an error instead of a crash.)
 *
 * ---- Writing: atomic, safe against concurrent writers ---------------------------------------------------------------
 * The file is written whole to a temp file in the destination folder (name <path>.<pid>-<thread>-<n>.tmp) and then
 * renamed over the destination (MoveFileExW, REPLACE_EXISTING): a reader sees the old complete file or the new
 * complete file. Two processes writing the same cache (two slots): both write complete files, the last rename wins.
 * Readers map the file with FILE_SHARE_DELETE, and a rename over a file that is mapped right now fails with
 * ACCESS_DENIED on Windows, so the writer retries for up to ~2 seconds, then gives up (returns false: a cache write
 * is best effort, like objcache's `except OSError: pass`). No fsync: a crash that tears the file is caught by the
 * checksum at the next read.
 *
 * ---- Append-only variant: the log ---------------------------------------------------------------------------------------
 * For tables that grow by a few entries per run and are shared by processes (objcache's PickleMemo: a key -> answer
 * table that Python re-read, merged and rewrote whole at exit), a log file avoids the read-merge-write race and the
 * rewrite of a large table: one header like the cache's but with magic "AVPL" (payload_len and payload_hash 0, the
 * header_crc covers it), then framed records appended one at a time:
 *        u32 marker 0x01525641 ("AVR\1"), u32 body_len, u32 check, body (one record of the typed fields above)
 *        check = low 32 bits xor high 32 bits of avp_hash64(body, body_len, AVP_LOG_SEED)
 * Each record is appended with ONE WriteFile on a handle opened FILE_APPEND_DATA, which the OS positions atomically
 * at the end of the file, so records of concurrent appenders never interleave. A reader takes every record whose
 * frame and check are valid; a torn or damaged record is skipped by scanning forward for the next marker, so a crash
 * mid-append loses that record only. Later records override earlier ones when the caller's table is keyed (replay
 * in order). To shrink a log, the caller replays it, keeps what it wants, frames it with avp_log_frame and replaces
 * the file with avp_log_write_file (atomic, as above).
 */
#pragma once
#include "str.h"

#define AVP_CACHE_FORMAT 1u
#define AVP_CACHE_SEED 0x4156504341434845ull /* "AVPCACHE" */
#define AVP_LOG_SEED 0x4156504C4F472121ull

/* ================================================================================================================
 * writing a payload
 * ================================================================================================================ */

typedef struct {
    AvpBytes b; /* the payload so far (heap) */
} AvpCacheWriter;
void avp_cw_init(AvpCacheWriter *w);
void avp_cw_clear(AvpCacheWriter *w); /* empties it, keeps the memory */
void avp_cw_free(AvpCacheWriter *w);
void avp_cw_u8(AvpCacheWriter *w, u8 v);
void avp_cw_u16(AvpCacheWriter *w, u16 v);
void avp_cw_u32(AvpCacheWriter *w, u32 v);
void avp_cw_u64(AvpCacheWriter *w, u64 v);
void avp_cw_i32(AvpCacheWriter *w, i32 v);
void avp_cw_i64(AvpCacheWriter *w, i64 v);
void avp_cw_f64(AvpCacheWriter *w, double v);
void avp_cw_bool(AvpCacheWriter *w, bool v);
void avp_cw_bytes(AvpCacheWriter *w, const void *p, size_t n); /* u32 length + bytes; n must fit in a u32 */
void avp_cw_str(AvpCacheWriter *w, AvpStr s);
void avp_cw_cz(AvpCacheWriter *w, const char *cz);             /* NULL is the empty string */
void avp_cw_count(AvpCacheWriter *w, size_t n);                 /* the u32 count that starts a vector / map */
/* A record: token = avp_cw_begin(w); ...fields...; avp_cw_end(w, token); (the length is patched in). */
size_t avp_cw_begin(AvpCacheWriter *w);
void avp_cw_end(AvpCacheWriter *w, size_t token);

/* ================================================================================================================
 * reading a payload
 * ================================================================================================================ */

typedef struct {
    const u8 *p;   /* next byte */
    const u8 *end; /* end of the record / payload */
    bool bad;      /* sticky: a read ran past the end, or a count did not fit */
} AvpCacheReader;
void avp_cr_init(AvpCacheReader *r, const void *p, size_t n);
bool avp_cr_ok(const AvpCacheReader *r);
size_t avp_cr_remaining(const AvpCacheReader *r);
u8 avp_cr_u8(AvpCacheReader *r);
u16 avp_cr_u16(AvpCacheReader *r);
u32 avp_cr_u32(AvpCacheReader *r);
u64 avp_cr_u64(AvpCacheReader *r);
i32 avp_cr_i32(AvpCacheReader *r);
i64 avp_cr_i64(AvpCacheReader *r);
double avp_cr_f64(AvpCacheReader *r);
bool avp_cr_bool(AvpCacheReader *r);
/* Views into the mapping/buffer (valid while it is): the empty string / NULL with n 0 after an error. */
AvpStr avp_cr_str(AvpCacheReader *r);
const u8 *avp_cr_bytes(AvpCacheReader *r, size_t *n);
void avp_cr_skip(AvpCacheReader *r, size_t n);
/* The u32 count of a vector / map whose elements take at least min_elem_bytes each: 0 and bad when the bytes left
 * cannot hold that many (so `avp_vec_reserve(&v, count)` is always safe). */
u32 avp_cr_count(AvpCacheReader *r, size_t min_elem_bytes);
/* A record: sub covers its fields; r moves past the whole record. After reading the fields call avp_cr_end(r, &sub)
 * so a short or damaged record marks r bad. Returns false (r bad) when the record does not fit. */
bool avp_cr_record(AvpCacheReader *r, AvpCacheReader *sub);
void avp_cr_end(AvpCacheReader *r, const AvpCacheReader *sub);

/* ================================================================================================================
 * cache files
 * ================================================================================================================ */

/* Writes the file atomically (see above). stamp may be NULL (stamp_n 0). False when it could not be written (the
 * folder is missing, disk full, the target stayed mapped by a reader past the retries); the old file, if any, is
 * untouched. The folder is not created: the caller made it (fs.h). */
bool avp_cache_write(const char *path, const char *kind, u32 schema, const void *stamp, size_t stamp_n,
                     const void *payload, size_t payload_n);
bool avp_cache_write_w(const char *path, const char *kind, u32 schema, const void *stamp, size_t stamp_n,
                       const AvpCacheWriter *w);

typedef struct {
    AvpCacheReader r;   /* reads the payload; use it directly or avp_cr_record() sub-readers */
    const u8 *payload;  /* the payload and its length */
    size_t payload_n;
    const u8 *stamp;    /* the stamp stored in the file (a view into the mapping) */
    size_t stamp_n;
    u32 schema;
    const u8 *base;     /* the whole file image (mapping) */
    size_t size;
    void *file, *mapping; /* OS handles; NULL when the image is caller memory (avp_cache_open_mem) */
} AvpCache;
/* Maps the file read-only and validates it: magic, format, header crc, kind, schema, stamp (when stamp != NULL;
 * with NULL any stamp is accepted and c->stamp tells it), exact length, payload checksum. False for every mismatch,
 * truncation, missing or unreadable file; *c is then empty and needs no close. The mapping stays valid until
 * avp_cache_close: copy out what you need, then close (an open view makes writers wait). */
bool avp_cache_open(AvpCache *c, const char *path, const char *kind, u32 schema, const void *stamp, size_t stamp_n);
/* The same validation over a file image already in memory (c->file and c->mapping stay NULL). */
bool avp_cache_open_mem(AvpCache *c, const void *image, size_t n, const char *kind, u32 schema, const void *stamp,
                        size_t stamp_n);
void avp_cache_close(AvpCache *c);
/* The bytes of a whole cache file (header + payload) appended to out: what avp_cache_write puts on disk. */
void avp_cache_encode(AvpBytes *out, const char *kind, u32 schema, const void *stamp, size_t stamp_n,
                      const void *payload, size_t payload_n);

/* ================================================================================================================
 * log files (append-only)
 * ================================================================================================================ */

/* Reading: maps the file, validates the header (magic "AVPL", format, kind, schema, stamp), then yields records. */
typedef struct {
    AvpCache c;       /* c.base / c.size; the header fields */
    size_t pos;       /* next unread offset */
    size_t skipped;   /* bytes passed over because a record was torn or damaged (a status line may show it) */
    size_t records;   /* records yielded so far */
} AvpLog;
bool avp_log_open(AvpLog *l, const char *path, const char *kind, u32 schema, const void *stamp, size_t stamp_n);
/* The next valid record's body as a reader (a view into the mapping); false at the end. */
bool avp_log_next(AvpLog *l, AvpCacheReader *body);
void avp_log_close(AvpLog *l);

/* Appending. Opens the log for appending, creating it (atomically, header only) when it is missing or its header
 * is not the expected one (another version / stamp: the old records are dropped, as a stale cache is). False when
 * it cannot be opened. One AvpLogAppender per thread; any number of processes may append to one file. */
typedef struct {
    void *file;
} AvpLogAppender;
bool avp_log_append_open(AvpLogAppender *a, const char *path, const char *kind, u32 schema, const void *stamp,
                         size_t stamp_n);
/* One WriteFile of one framed record. False if the write failed (the record may be lost; the log stays valid). */
bool avp_log_append(AvpLogAppender *a, const void *body, size_t n);
void avp_log_append_close(AvpLogAppender *a);

/* Building a whole log in memory (for compaction): header via avp_log_write_file, records via avp_log_frame. */
void avp_log_frame(AvpBytes *out, const void *body, size_t n);
bool avp_log_write_file(const char *path, const char *kind, u32 schema, const void *stamp, size_t stamp_n,
                        const void *frames, size_t frames_n);
