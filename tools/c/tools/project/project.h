/* tools/project.py in C: the classes configure.py instantiates and the two entry points it calls.
 * DESIGN.md: tools/c/tools/configure/DESIGN.md. Values: pv.h.
 *
 * Ownership: the constructors (project_new_*) are written with the configure interpreter (pv_project.c); the
 * generator and the progress printer (everything else) with the port of project.py proper. */
#pragma once
#include "pv.h"

/* How the plumbing was invoked (configure.py's sys.argv) and where the C tools the graph runs live. */
typedef struct {
    const char *script;       /* sys.argv[0] as given ("configure.py") */
    const char *const *argv;  /* sys.argv[1:]: the script's arguments, unchanged (configure_args) */
    size_t argc;
    const char *ctools;       /* absolute directory of configure.exe and the other C tools every tool step runs */
} ProjectEnv;

/* Object(completed, name, **options): an instance "Object" with attributes name, completed and options (the
 * defaults of Object.__init__ in their order, then `options` merged in its order), plus src_path, asm_path,
 * src_obj_path, asm_obj_path and ctx_path set to None. options may be NULL (no keyword arguments). */
Pv *project_new_object(AvpArena *a, bool completed, AvpStr name, const Pv *options);
/* ProgressCategory(id, name). */
Pv *project_new_progress_category(AvpArena *a, AvpStr id, AvpStr name);
/* ProjectConfig(): every attribute of ProjectConfig.__init__ with its default, in __init__'s order. */
Pv *project_new_config(AvpArena *a);
/* is_windows(): true in this build. */
bool project_is_windows(void);

/* generate_build(config): writes build.ninja, objdiff.json and compile_commands.json (when configured) in the
 * current directory, printing what Python prints. Returns the process exit code Python would have (0, or the code
 * of a sys.exit inside project.py; the message is already on stderr). */
int project_generate_build(AvpArena *a, Pv *config, const ProjectEnv *env);
/* calculate_progress(config): prints the progress report. Exit code as above. */
int project_calculate_progress(AvpArena *a, Pv *config, const ProjectEnv *env);
