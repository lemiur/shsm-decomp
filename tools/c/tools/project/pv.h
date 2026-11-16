/* Python values for the build plumbing (tools/c/tools/project, DESIGN.md in tools/c/tools/configure).
 *
 * configure.exe executes configure.py (the interpreter, tools/c/tools/configure/cfg_*.c) into these values, and the
 * port of tools/project.py (generate_build, calculate_progress, Object.resolve) reads them. Python's object model is
 * kept where the output can see it: lists and dicts are shared by reference, dicts keep insertion order, an instance
 * is a class name plus an insertion-ordered attribute dict. Every value lives in an arena; nothing is freed.
 *
 * Paths are Windows pathlib paths (PureWindowsPath): a PV_PATH holds str(path) (backslashes, "." for the empty
 * path), and the pv_path_* functions reproduce pathlib's results on that form. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "avp/arena.h"
#include "avp/str.h"

typedef enum {
    PV_NONE,
    PV_BOOL,
    PV_INT,
    PV_STR,      /* UTF-8 */
    PV_PATH,     /* str(PureWindowsPath(...)) */
    PV_LIST,
    PV_TUPLE,
    PV_DICT,     /* string keys only (all configure.py and project.py dicts), insertion order */
    PV_INSTANCE, /* Object, ProjectConfig, ProgressCategory, argparse.Namespace, ... */
    PV_OPAQUE,   /* interpreter-private (functions, built-ins, bound methods, modules): tag + pointer */
} PvKind;

typedef struct Pv Pv;
typedef struct { Pv **v; size_t n, cap; } PvList;               /* list and tuple items */
typedef struct { AvpStr *keys; Pv **vals; size_t n, cap; } PvDict;
typedef struct { const char *cls; PvDict *attrs; } PvInstance;

struct Pv {
    PvKind kind;
    union {
        bool b;
        int64_t i;
        AvpStr s;          /* PV_STR, PV_PATH */
        PvList *list;      /* PV_LIST, PV_TUPLE */
        PvDict *dict;      /* PV_DICT */
        PvInstance *inst;  /* PV_INSTANCE */
        struct { const char *tag; void *p; } opaque;
    } u;
};

/* ---- construction ---- */
Pv *pv_none(void);                         /* the shared None */
Pv *pv_bool(bool b);                       /* the shared True / False */
Pv *pv_int(AvpArena *a, int64_t i);
Pv *pv_str(AvpArena *a, AvpStr s);         /* copies s */
Pv *pv_strz(AvpArena *a, const char *s);
Pv *pv_path(AvpArena *a, AvpStr s);        /* Path(s): normalised as PureWindowsPath(s), see pv_path_norm */
Pv *pv_list(AvpArena *a);                  /* [] */
Pv *pv_tuple(AvpArena *a);                 /* () (fill with pv_list_push before sharing it) */
Pv *pv_dict(AvpArena *a);                  /* {} */
Pv *pv_instance(AvpArena *a, const char *cls);
Pv *pv_opaque(AvpArena *a, const char *tag, void *p);

/* ---- lists, tuples, dicts, attributes ---- */
void pv_list_push(AvpArena *a, Pv *list, Pv *item);           /* append (list or tuple under construction) */
void pv_list_extend(AvpArena *a, Pv *list, const Pv *items);  /* extend by a list or tuple */
Pv *pv_list_copy(AvpArena *a, const Pv *list);                /* list(x): a new list of the same items */
size_t pv_len(const Pv *v);                                   /* len() of str, list, tuple, dict */
Pv *pv_dict_get(const Pv *dict, AvpStr key);                  /* NULL when absent */
void pv_dict_set(AvpArena *a, Pv *dict, AvpStr key, Pv *val);  /* insert at the end, or replace in place */
Pv *pv_dict_copy(AvpArena *a, const Pv *dict);
Pv *pv_getattr(const Pv *inst, const char *name);             /* NULL when absent */
void pv_setattr(AvpArena *a, Pv *inst, const char *name, Pv *val);
bool pv_isinstance(const Pv *v, const char *cls);

/* ---- Python semantics ---- */
bool pv_truthy(const Pv *v);                       /* bool(v) */
bool pv_eq(const Pv *x, const Pv *y);              /* x == y (path == path compares the normalised forms, as
                                                      PureWindowsPath: case-insensitively) */
AvpStr pv_to_str(AvpArena *a, const Pv *v);         /* str(v): str of a path is its form, of a list its repr */
AvpStr pv_repr(AvpArena *a, const Pv *v);           /* repr(v) for None, bools, ints, str (Python quoting), paths
                                                      (WindowsPath('...')), lists, tuples, dicts */
const char *pv_kind_name(const Pv *v);              /* type(v).__name__: "NoneType", "str", "WindowsPath", ... */

/* ---- Windows pathlib (all take and return the str() form) ---- */
AvpStr pv_path_norm(AvpArena *a, AvpStr s);                   /* str(PureWindowsPath(s)) */
AvpStr pv_path_join(AvpArena *a, AvpStr base, AvpStr other);  /* base / other (an absolute or drive-rooted other
                                                                 replaces base as pathlib does) */
AvpStr pv_path_parent(AvpArena *a, AvpStr p);                 /* .parent */
AvpStr pv_path_name(AvpStr p);                                /* .name */
AvpStr pv_path_suffix(AvpStr p);                              /* .suffix ("" when none) */
AvpStr pv_path_stem(AvpStr p);                                /* .stem */
AvpStr pv_path_with_suffix(AvpArena *a, AvpStr p, AvpStr suffix); /* .with_suffix(suffix); {NULL, 0} where
                                                                     pathlib raises ValueError (empty stem, invalid
                                                                     suffix or name) */
AvpStr pv_path_as_posix(AvpArena *a, AvpStr p);               /* .as_posix() */
bool pv_path_is_absolute(AvpStr p);                           /* .is_absolute() */
