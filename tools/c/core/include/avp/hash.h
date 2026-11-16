/* Hashing. Replaces what the Python tools got from hashlib (md5, sha1, sha256; file_digest; update() streaming) and
 * zlib.crc32, plus a fast non-cryptographic hash for in-memory tables.
 *
 * The cryptographic digests go through Windows CNG (BCrypt: SHA-NI / AVX2 where the CPU has it, picked by the OS).
 * One algorithm handle per algorithm is opened once (thread-safe, InitOnce) and shared by every thread; a digest of
 * a buffer is one BCryptHash call, an incremental hash is a reusable BCrypt hash object. The bytes are the same as
 * hashlib's (the algorithms are fixed by their standards); `hashlib.md5(x, usedforsecurity=False)` is `avp_md5`.
 *
 *   hashlib.sha256(data).digest()          avp_sha256(data, n, out)
 *   hashlib.sha256(data).hexdigest()       avp_sha256_hex(arena, data, n)
 *   h = hashlib.sha1(); h.update(a); ...   AvpHasher h; avp_hasher_init(&h, AVP_HASH_SHA1); avp_hasher_update(...)
 *   h.digest() / h.hexdigest()             avp_hasher_final(&h, out)   (ends the hash; avp_hasher_peek keeps it)
 *   h.copy()                               avp_hasher_copy
 *   hashlib.file_digest(f, "sha256")       avp_file_digest(AVP_HASH_SHA256, path, out)  (memory-mapped, no copy)
 *   zlib.crc32(data, value)                avp_crc32(value, data, n)   (a table CRC-32, the same value as CPython's)
 *   .hexdigest()[:12]                      avp_hex(a, digest, 6)
 *
 * avp_hash64 is NOT cryptographic and NOT Python's hash(): it is for the tools' own tables and for the cache
 * checksum (cache.h). Its output is frozen (documented at the function) because cache files store it.
 *
 * Failures of CNG (a missing algorithm provider cannot happen on the supported Windows; out of memory) are
 * internal errors (avp_panic), not return codes; only file digests can fail (unreadable file) and say so. */
#pragma once
#include "str.h"

typedef enum { AVP_HASH_MD5, AVP_HASH_SHA1, AVP_HASH_SHA256, AVP_HASH_SHA512 } AvpHashAlg;
#define AVP_MD5_LEN 16
#define AVP_SHA1_LEN 20
#define AVP_SHA256_LEN 32
#define AVP_SHA512_LEN 64
#define AVP_HASH_MAX_LEN 64

size_t avp_hash_len(AvpHashAlg alg); /* digest_size */

/* ---- one-shot ---- */
void avp_hash(AvpHashAlg alg, const void *p, size_t n, u8 *out); /* out holds avp_hash_len(alg) bytes */
void avp_md5(const void *p, size_t n, u8 out[16]);
void avp_sha1(const void *p, size_t n, u8 out[20]);
void avp_sha256(const void *p, size_t n, u8 out[32]);
void avp_sha512(const void *p, size_t n, u8 out[64]);

/* Lower-case hex of n bytes, NUL-terminated, from the arena (2n + 1 bytes). */
char *avp_hex(AvpArena *a, const u8 *p, size_t n);
/* Same into a caller buffer of at least 2n + 1 bytes; returns 2n. */
size_t avp_hex_to(const u8 *p, size_t n, char *out);
/* hashlib.<alg>(data).hexdigest() */
char *avp_hash_hex(AvpArena *a, AvpHashAlg alg, const void *p, size_t n);
char *avp_md5_hex(AvpArena *a, const void *p, size_t n);
char *avp_sha1_hex(AvpArena *a, const void *p, size_t n);
char *avp_sha256_hex(AvpArena *a, const void *p, size_t n);

/* ---- incremental ----
 * A zeroed or init'ed AvpHasher is owned by one thread at a time. avp_hasher_final writes the digest and releases
 * the hash; avp_hasher_free releases a hash that is abandoned. After either, the struct may be init'ed again. */
typedef struct {
    void *h;         /* BCRYPT_HASH_HANDLE */
    AvpHashAlg alg;
} AvpHasher;
void avp_hasher_init(AvpHasher *h, AvpHashAlg alg);
void avp_hasher_update(AvpHasher *h, const void *p, size_t n); /* any n, also over 4 GiB */
void avp_hasher_final(AvpHasher *h, u8 *out);
void avp_hasher_peek(const AvpHasher *h, u8 *out); /* the digest so far; the hash stays usable (digest() then update()) */
void avp_hasher_copy(AvpHasher *dst, const AvpHasher *src); /* hashlib's .copy() */
void avp_hasher_free(AvpHasher *h);

/* ---- files ---- */
/* The digest of a whole file, read through a read-only mapping (an empty file hashes as the empty input). False
 * when the file cannot be opened or read (nothing written to out). Paths are UTF-8 with / or \. */
bool avp_file_digest(AvpHashAlg alg, const char *path, u8 *out);
/* The hex digest from the arena, or NULL when the file cannot be read. */
char *avp_file_digest_hex(AvpArena *a, AvpHashAlg alg, const char *path);

/* ---- CRC-32 ----
 * zlib.crc32(data, value): value is the running crc (0 to start); the result is the unsigned 32-bit crc. */
u32 avp_crc32(u32 value, const void *p, size_t n);

/* ---- fast non-cryptographic 64-bit hash ----
 * A folded-multiply construction (the family of wyhash / rapidhash: 64x64 -> 128-bit multiplies whose halves are
 * xored), written for this project: 48 bytes per round on three independent lanes for long inputs, two overlapping
 * reads for short ones, the length mixed into the final multiply. About 10-20 GB/s on one core for long inputs and
 * a few ns for short keys. Platform-independent: all reads are little-endian, so the result is the same on every
 * machine, and FROZEN: cache.h stores avp_hash64(payload, n, AVP_CACHE_SEED) in files, so a change here is a cache
 * format change (hash_test.c pins vectors). Not collision-resistant against an adversary: never for security. */
u64 avp_hash64(const void *p, size_t n, u64 seed);
u64 avp_hash64_str(AvpStr s, u64 seed);
/* A good 64-bit finaliser for integer keys (a bijection of u64: distinct inputs give distinct outputs). */
u64 avp_hash64_u64(u64 x);
