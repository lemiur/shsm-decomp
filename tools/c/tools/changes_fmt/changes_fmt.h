/* changes_fmt: formatting of objdiff-cli `report changes` JSON (ports tools/changes_fmt.py). */
#pragma once
#include "avp/avp.h"

/* A JSON value as the Python holds it in a comparison: int (bool included), float, str, or anything else (which
 * cannot be ordered). Integers beyond 64 bits are kept as doubles (D-changes_fmt-3). */
typedef enum { CF_INT, CF_FLOAT, CF_STR, CF_OTHER } CfKind;
typedef struct {
    CfKind kind;
    i64 i;
    double f;
    AvpStr s;          /* CF_STR: the string; CF_OTHER: the Python type name ("NoneType", "list", "dict") */
} CfValue;

/* One (name, key, from, to) tuple of the Python. has_name false is name None ("Total"); bad_type is the Python type
 * name of a name that is neither str nor None (every use of it raises TypeError). */
typedef struct {
    bool has_name;
    AvpStr name;
    const char *bad_type;
    AvpStr key;        /* the key with "_percent" removed */
    CfValue from, to;
} CfChange;
typedef AVP_VEC(CfChange) CfChanges;

/* Errors a Python traceback would carry are avp_throw(AVP_ERR_OTHER, "<ExceptionClass>: <text>"). */

CfValue cf_value_from_json(const AvpJson *v);       /* NULL: the default 0.0 */
bool cf_greater(CfValue a, CfValue b);              /* a > b; TypeError for unordered pairs */
AvpStr cf_format_float(AvpArena *a, CfValue v);     /* format_float: "%6.2f" with 99.99 for (99.99, 100.0) */

/* get_changes() over the parsed document. */
void cf_get_changes(const AvpJson *root, CfChanges *regressions, CfChanges *progressions);

/* generate_changes_plaintext / generate_changes_markdown (UTF-8, no trailing newline; "" for no changes). */
AvpStr cf_plaintext(AvpArena *a, const CfChange *changes, size_t n);
AvpStr cf_markdown(AvpArena *a, const CfChange *changes, size_t n, const char *description);
