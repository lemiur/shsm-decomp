/* Internals of the port of tools/project.py (project_*.c). */
#pragma once
#include <stdbool.h>

#include "avp/avp.h"
#include "avp/json.h"
#include "avp/map.h"
#include "ninja_syntax.h"
#include "project.h"

typedef AVP_VEC(const char *) PStrs;

/* An Object after Object.resolve: the instance (its options dict is shared with the generator, which edits
 * extra_cflags as the Python does) and its resolved paths (str(Path) forms; NULL for None). */
typedef struct {
    Pv *inst;
    Pv *options;
    const char *name;
    bool completed;
    const char *src_path, *asm_path, *src_obj_path, *asm_obj_path, *ctx_path;
} PObj;

typedef struct {
    AvpArena *a;
    Pv *config;
    const ProjectEnv *env;
    AvpMap *objects; /* name -> PObj* (config.objects(), insertion order) */
    AvpJson *build_config; /* load_build_config; NULL for None */
} PCtx;

/* ---- helpers (project_util.c) ---- */
AVP_NORETURN void proj_exit(const char *fmt, ...); /* sys.exit("message") */
Pv *proj_attr(const PCtx *c, const char *name);    /* getattr(config, name); a missing attribute is fatal */
const char *proj_path(AvpArena *a, const char *s); /* str(Path(s)) */
const char *proj_join(AvpArena *a, const char *base, const char *other); /* str(Path(base) / other) */
const char *proj_pv_path(AvpArena *a, const Pv *v); /* str(Path(v)) for a str or Path value; NULL for None */
const char *proj_pv_str(AvpArena *a, const Pv *v);  /* str(v) */
const char *proj_with_suffix(AvpArena *a, const char *p, const char *suffix); /* fatal ValueError as Python */
const char *proj_list2cmdline(AvpArena *a, size_t n, const char *const *args); /* subprocess.list2cmdline */
const char *proj_cmd_path(AvpArena *a, const char *path); /* cmd_path */
const char *proj_chain(AvpArena *a, const char *command); /* chain (Windows: cmd /c "...") */
const char *proj_relpath(AvpArena *a, const char *path); /* os.path.relpath(os.path.abspath(path)) */
bool proj_file_is_asm(const char *p);
bool proj_file_is_c(const char *p);
bool proj_file_is_cpp(const char *p);
bool proj_path_eq(const char *x, const char *y); /* Path == Path (case-insensitive, as PureWindowsPath) */
const char *proj_flags_str(AvpArena *a, const Pv *flags); /* make_flags_str */
bool proj_startswith(const char *s, const char *prefix);
void proj_check_path_case(AvpArena *a, const char *path);
PObj *proj_object(const PCtx *c, AvpStr name); /* objects.get(name) */
/* serialize_paths of a value from configure.py (custom build steps): str, Path, None, or a list of them */
NinjaPaths proj_pv_ninja(AvpArena *a, const Pv *v);
/* json.load(path); fatal (as a traceback would be) on a decode error */
AvpJson *proj_load_json(AvpArena *a, const char *path);
/* open(path, "w", encoding="utf-8").write(text) */
void proj_write_text(AvpArena *a, const char *path, AvpStr text);

/* ---- the generator ---- */
void proj_resolve_objects(PCtx *c);            /* config.objects() */
void proj_load_build_config(PCtx *c, const char *path);
void proj_generate_build_ninja(PCtx *c);
void proj_generate_objdiff_config(PCtx *c);
void proj_generate_compile_commands(PCtx *c);
void proj_validate(PCtx *c);
const char *proj_out_path(const PCtx *c); /* config.out_path() */
