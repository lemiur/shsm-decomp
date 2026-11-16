/* JSON as Python's json module does it, byte for byte: json.loads / json.load into a value tree, and json.dumps /
 * json.dump out of one (CPython 3.14).
 *
 * The tree. An AvpJson value is null, bool, int, float, string, array or object, allocated from an arena (the
 * whole tree lives as long as the arena). Objects are Python dicts: members keep insertion order, setting a key
 * that exists replaces the value and keeps the key's position (so json.loads of {"a":1,"b":2,"a":3} gives a=3
 * first, b=2 second), and a deleted key that is set again goes to the end. Lookup is O(1) once an object has more
 * than 8 members. Ints are an i64, or, when Python's unbounded int does not fit, the exact decimal digits (kept
 * and written back unchanged). Floats are doubles, correctly rounded from the literal; NaN, Infinity and
 * -Infinity are accepted and written as json does. Strings are UTF-8; a lone surrogate (JSON "\ud800" gives a
 * Python str holding it) is kept as its 3-byte generalized-UTF-8 form so that it survives a round trip with
 * ensure_ascii; a valid 😀 pair is one code point, as in Python. null, true and false are shared
 * immutable nodes: never modify what avp_json_null()/avp_json_bool() return.
 *
 * Reading (json.loads). avp_json_parse gives the tree or NULL with *err filled; err == NULL throws
 * AVP_ERR_FORMAT with Python's text instead (use it inside AVP_TRY where the Python wrapped json.load in
 * try/except). The messages, line, column and char position are JSONDecodeError's: "Expecting value: line 1
 * column 1 (char 0)"; positions count code points, as Python's str indices do. Strict mode as the defaults
 * (control characters in strings are errors), duplicate keys last-wins, depth limited as the C stack of
 * CPython is (about 10000 levels; the exact limit of CPython depends on the stack and is not reproduced).
 * Bytes that are not valid UTF-8 are AVP_JSON_E_UNICODE (what reading the file in Python raises before json sees
 * it; the message is modelled on UnicodeDecodeError's but not guaranteed identical). A text starting with the BOM
 * U+FEFF is the "Unexpected UTF-8 BOM (decode using utf-8-sig)" error, which is what
 * json.load(open(path, encoding="utf-8")) does; AVP_JSON_STRIP_BOM is the utf-8-sig behaviour. The hooks the
 * project's tools pass to json.loads (parse_constant raising, parse_float raising on non-finite numbers,
 * object_pairs_hook rejecting duplicate keys) are flags.
 *
 * Writing (json.dumps / json.dump). avp_json_dumps and the streaming avp_jsonw_* functions give the bytes
 * json.dumps gives for indent (none, a width, or a string), separators (the defaults (", ", ": "), or (",", ": ")
 * when indent is given, or custom ones), sort_keys (by code point), ensure_ascii (escapes as \uXXXX with
 * lower-case hex and surrogate pairs, DEL escaped; or raw UTF-8), allow_nan, floats as repr() (avp_float_repr in
 * fmt.h), ints as digits, "[]" and "{}" for empty containers even with indent. A zeroed AvpJsonDumpOpts is
 * json.dumps(x) with no options. json.dumps(x, default=str) and the like are the caller's job: convert the
 * value while building the tree.
 *
 * The streaming writer is the fast path for tools that only write records out: the same bytes as json.dumps of
 * the equivalent tree, without building it. It cannot sort keys: with sort_keys the caller writes the keys in
 * sorted order itself (avp_jsonw_tree sorts when it writes a tree). */
#pragma once
#include "str.h"
#include <stdio.h>

typedef enum {
    AVP_JSON_NULL,
    AVP_JSON_BOOL,
    AVP_JSON_INT,
    AVP_JSON_FLOAT,
    AVP_JSON_STRING,
    AVP_JSON_ARRAY,
    AVP_JSON_OBJECT
} AvpJsonKind;

typedef struct AvpJson AvpJson;
typedef struct {
    AvpStr key;
    AvpJson *val;
} AvpJsonMember;

struct AvpJson {
    u8 kind;     /* AvpJsonKind */
    u8 big;      /* INT only: the value does not fit in an i64; u.str holds its exact decimal text */
    bool b;      /* BOOL */
    union {
        i64 i;
        double f;
        AvpStr str; /* STRING; INT with big */
        struct {
            AvpJson **v;
            size_t n, cap;
        } arr;
        struct {
            AvpJsonMember *v; /* in insertion order */
            size_t n, cap;
            u32 *index;       /* open-addressing table of member numbers + 1 (NULL up to 8 members) */
            u32 index_cap;
        } obj;
    } u;
};

/* ---- errors ---- */
enum {
    AVP_JSON_E_NONE = 0,
    AVP_JSON_E_DECODE,    /* json.JSONDecodeError: msg, lineno, colno, pos */
    AVP_JSON_E_VALUE,     /* a ValueError without position (int digit limit, hooks, NaN with allow_nan=False) */
    AVP_JSON_E_RECURSION, /* RecursionError (nesting too deep) */
    AVP_JSON_E_UNICODE    /* the input bytes are not valid UTF-8 (UnicodeDecodeError when the file was read) */
};
typedef struct {
    int kind;
    AvpStr msg; /* JSONDecodeError.msg ("Expecting value"), or the ValueError text; allocated from the arena */
    i64 lineno, colno, pos; /* DECODE: 1-based line and column, 0-based char position (code points) */
} AvpJsonError;
/* str(exception): "msg: line L column C (char P)" for DECODE, else the message. */
AvpStr avp_json_error_str(AvpArena *a, const AvpJsonError *err);

/* ---- reader ---- */
enum {
    AVP_JSON_STRIP_BOM = 1,         /* a leading BOM is skipped (encoding="utf-8-sig") instead of an error */
    AVP_JSON_REJECT_CONSTANTS = 2,  /* parse_constant raising ValueError("invalid JSON constant: NaN") */
    AVP_JSON_REJECT_NONFINITE = 4,  /* parse_float raising ValueError("nonfinite JSON number") on inf (1e999) */
    AVP_JSON_REJECT_DUP_KEYS = 8,   /* object_pairs_hook raising ValueError("duplicate JSON key: k") */
    AVP_JSON_BYTES = 16             /* json.loads(bytes): a UTF-8 BOM is skipped silently (utf-16/32 not handled) */
};
AvpJson *avp_json_parse(AvpArena *a, AvpStr text, AvpJsonError *err);
AvpJson *avp_json_parse_ex(AvpArena *a, AvpStr text, unsigned flags, AvpJsonError *err);

/* ---- building ---- */
AvpJson *avp_json_null(void);
AvpJson *avp_json_bool(bool b);
AvpJson *avp_json_int(AvpArena *a, i64 v);
AvpJson *avp_json_uint(AvpArena *a, u64 v);              /* digits when above INT64_MAX */
AvpJson *avp_json_int_digits(AvpArena *a, AvpStr digits); /* "-?(0|[1-9][0-9]*)", any length */
AvpJson *avp_json_float(AvpArena *a, double v);
AvpJson *avp_json_str(AvpArena *a, AvpStr s);            /* copies s */
AvpJson *avp_json_str_ref(AvpArena *a, AvpStr s);        /* keeps s: it must outlive the tree */
AvpJson *avp_json_strz(AvpArena *a, const char *cz);
AvpJson *avp_json_arr(AvpArena *a);
AvpJson *avp_json_arr_cap(AvpArena *a, size_t cap);
AvpJson *avp_json_obj(AvpArena *a);

/* list.append / list[i] / len */
void avp_json_arr_push(AvpArena *a, AvpJson *arr, AvpJson *v);
size_t avp_json_len(const AvpJson *v); /* items of an array, members of an object, code points NOT: bytes of a string */
AvpJson *avp_json_at(const AvpJson *arr, size_t i); /* NULL when out of range */

/* dict[key] = v: replaces the value of an existing key (position kept) or appends. set copies the key, set_ref
 * keeps it. Returns the previous value or NULL. */
AvpJson *avp_json_set(AvpArena *a, AvpJson *obj, AvpStr key, AvpJson *v);
AvpJson *avp_json_set_ref(AvpArena *a, AvpJson *obj, AvpStr key, AvpJson *v);
/* del dict[key]: true when it existed (later members move up one place). */
bool avp_json_del(AvpJson *obj, AvpStr key);
const AvpJsonMember *avp_json_member(const AvpJson *obj, size_t i); /* the i-th member in insertion order, or NULL */

/* ---- reading values ---- */
/* dict.get(key): NULL when absent (or when obj is not an object). */
AvpJson *avp_json_get(const AvpJson *obj, AvpStr key);
AvpJson *avp_json_getz(const AvpJson *obj, const char *key);
/* dict.get(key, dflt) */
AvpJson *avp_json_get_or(const AvpJson *obj, AvpStr key, AvpJson *dflt);
bool avp_json_has(const AvpJson *obj, AvpStr key);
/* The typed accessors return the fallback when the value is NULL or of another kind (ints and floats are
 * interchangeable as numbers: avp_json_f64 of an int gives float(int); avp_json_i64 of a float gives the fallback;
 * a big int gives the fallback from avp_json_i64). */
bool avp_json_is_null(const AvpJson *v);   /* true for NULL too */
bool avp_json_is_str(const AvpJson *v);
bool avp_json_is_arr(const AvpJson *v);
bool avp_json_is_obj(const AvpJson *v);
bool avp_json_is_int(const AvpJson *v);
bool avp_json_is_num(const AvpJson *v);
i64 avp_json_i64(const AvpJson *v, i64 fallback);
double avp_json_f64(const AvpJson *v, double fallback);
bool avp_json_bool_or(const AvpJson *v, bool fallback);
AvpStr avp_json_str_or(const AvpJson *v, AvpStr fallback);
/* d.get(key, fallback) followed by the accessor. */
i64 avp_json_get_i64(const AvpJson *obj, const char *key, i64 fallback);
double avp_json_get_f64(const AvpJson *obj, const char *key, double fallback);
bool avp_json_get_bool(const AvpJson *obj, const char *key, bool fallback);
AvpStr avp_json_get_str(const AvpJson *obj, const char *key, AvpStr fallback);
/* Python truthiness: not null/false/0/0.0/""/[]/{} (a NULL pointer is None). */
bool avp_json_truthy(const AvpJson *v);

/* ---- writer ---- */
typedef struct {
    bool indented;      /* indent is not None */
    int indent_width;   /* indent=N: N spaces (<= 0 gives just the newlines) */
    AvpStr indent;      /* indent="str": used instead of the width when .p is not NULL */
    bool custom_separators;
    AvpStr item_sep, key_sep; /* separators=(item, key); the defaults are (", ", ": ") and (",", ": ") with indent */
    bool sort_keys;
    bool raw_unicode;   /* ensure_ascii=False */
    bool reject_nan;    /* allow_nan=False: NaN/inf give a ValueError("Out of range float values are not JSON compliant: nan") */
    AvpJsonError *err;  /* receives that error when non-NULL (kind VALUE); the dump then returns failure */
} AvpJsonDumpOpts;
#define AVP_JSON_INDENT(n) ((AvpJsonDumpOpts){.indented = true, .indent_width = (n)})
#define AVP_JSON_COMPACT   ((AvpJsonDumpOpts){.custom_separators = true, .item_sep = {",", 1}, .key_sep = {":", 1}})

/* json.dumps. On failure (reject_nan met a non-finite float) the result has p == NULL. */
AvpStr avp_json_dumps(AvpArena *a, const AvpJson *v, const AvpJsonDumpOpts *opts);
/* Appends the dump to out (the caller adds the trailing "\n" the Python wrote); false on failure. */
bool avp_json_dump_buf(AvpBuf *out, const AvpJson *v, const AvpJsonDumpOpts *opts);
/* json.dump(v, file): writes with fwrite in chunks; false on a write error or a failure of the dump. */
bool avp_json_dump_file(FILE *f, const AvpJson *v, const AvpJsonDumpOpts *opts);

/* The string escaper alone: json.dumps(s) / json.dumps(s, ensure_ascii=False), quotes included. */
void avp_json_put_string(AvpBuf *out, AvpStr s, bool raw_unicode);

/* ---- streaming writer ----
 *     AvpBuf b = {0};
 *     AvpJsonW w;
 *     avp_jsonw_init(&w, &b, &AVP_JSON_COMPACT);      // opts may be NULL
 *     avp_jsonw_begin_object(&w);
 *     avp_jsonw_key(&w, AVP_S("t")); avp_jsonw_i64(&w, 5);
 *     avp_jsonw_end_object(&w);
 * A key is followed by exactly one value (any value, a container included). After the top-level value, w.out
 * holds json.dumps' bytes. avp_jsonw_set_spill makes the writer hand its buffer to fn and clear it whenever it
 * reaches threshold bytes (writing a big file without holding it); avp_jsonw_finish spills the rest.
 * Misuse (a value where a key is due) is an internal error (avp_panic). */
typedef bool (*AvpJsonSpillFn)(void *ctx, const char *p, size_t n);
typedef struct {
    AvpBuf *out;
    AvpJsonDumpOpts o;
    AvpStr item_sep, key_sep, indent_unit;
    bool failed;
    u8 *stack; /* per open container: 1 array, 2 object; bit 0x80 once it has an item; 0x40: a key is pending */
    size_t depth, stack_cap;
    u8 inline_stack[32];
    size_t spill_threshold;
    AvpJsonSpillFn spill;
    void *spill_ctx;
} AvpJsonW;
void avp_jsonw_init(AvpJsonW *w, AvpBuf *out, const AvpJsonDumpOpts *opts);
void avp_jsonw_set_spill(AvpJsonW *w, size_t threshold, AvpJsonSpillFn fn, void *ctx);
bool avp_jsonw_finish(AvpJsonW *w); /* spills what is left, frees the writer's stack; false after a failure */
void avp_jsonw_begin_object(AvpJsonW *w);
void avp_jsonw_end_object(AvpJsonW *w);
void avp_jsonw_begin_array(AvpJsonW *w);
void avp_jsonw_end_array(AvpJsonW *w);
void avp_jsonw_key(AvpJsonW *w, AvpStr key);
void avp_jsonw_null(AvpJsonW *w);
void avp_jsonw_bool(AvpJsonW *w, bool b);
void avp_jsonw_i64(AvpJsonW *w, i64 v);
void avp_jsonw_u64(AvpJsonW *w, u64 v);
void avp_jsonw_int_digits(AvpJsonW *w, AvpStr digits); /* an int of any size, written as given */
void avp_jsonw_f64(AvpJsonW *w, double v);
void avp_jsonw_str(AvpJsonW *w, AvpStr s);
void avp_jsonw_strz(AvpJsonW *w, const char *cz);
void avp_jsonw_tree(AvpJsonW *w, const AvpJson *v); /* a whole tree as one value (keys sorted when o.sort_keys) */
/* key + value in one call */
void avp_jsonw_kv_str(AvpJsonW *w, const char *key, AvpStr s);
void avp_jsonw_kv_i64(AvpJsonW *w, const char *key, i64 v);
void avp_jsonw_kv_f64(AvpJsonW *w, const char *key, double v);
void avp_jsonw_kv_bool(AvpJsonW *w, const char *key, bool b);
