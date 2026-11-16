/* The repository model (tools/common/repo.py): the root of a checkout (main or a lane worktree), its version name and
 * standard paths, and the unit name forms. Paths are what os.path gives on Windows (os.path.join: backslashes). */
#pragma once
#include "avp/str.h"

typedef struct ShsmRepo {
    const char *root;               /* os.path.abspath of the checkout */
    const char *version;              /* "R5WEA4" */
    const char *config_dir, *symbols_txt, *splits_txt, *config_yml, *build_sha1, *baseline_report_json;
    const char *configure_py, *objdiff_json, *build_ninja;
    const char *build_dir, *version_build_dir, *asm_dir, *obj_dir, *src_obj_dir, *report_json, *main_dol;
    const char *retail_dol, *libraries_tsv, *score_history_db, *index_dir;
    const char *tools_dir, *bin_dir, *compilers_dir, *sjiswrap, *src_dir, *include_dir;
} ShsmRepo;

/* repo.find_root(start): the nearest folder at or above start (NULL: the cwd) holding configure.py, config/ and
 * tools/. Not found: an explicit start raises SHSM_EXC_FILE_NOT_FOUND "not inside a checkout (no
 * configure.py/config/tools): <start>"; else the checkout of the executable (<root>/build/tools/x.exe), else
 * "cannot find the repository root from <cwd>". */
const char *shsm_find_root(const char *start);
/* repo.read_version(root): VERSIONS[DEFAULT_VERSION] of configure.py, else the one config/<v>/config.yml; raises
 * SHSM_EXC_VALUE "cannot determine the version name in <root>". */
const char *shsm_read_version(const char *root);
/* repo.retail_dol_path: <root>/<object_base>/<object> from config.yml (normpath), NULL when config.yml names none. */
const char *shsm_retail_dol_path(const char *root, const char *config_yml);
/* Repo(root, version): every path (existence not checked). version NULL: read_version. */
ShsmRepo *shsm_repo_new(const char *root, const char *version);
/* Repo.find(root): the Repo of root (NULL: the cwd's checkout), cached per normcase(root). */
ShsmRepo *shsm_repo_find(const char *root);

/* Repo.tool(name): tools/bin/<name>.exe ("dtk", "objdiff-cli", "ninja"; ".exe" not doubled). */
const char *shsm_repo_tool(AvpArena *a, const ShsmRepo *r, const char *name);
/* Repo.compiler(mw_version): tools/compilers/<Wii/1.0 split at / and \>/mwcceppc.exe. mw_version NULL raises
 * SHSM_EXC_TYPE as re.split(None) did. exe NULL: "mwcceppc.exe". */
const char *shsm_repo_compiler(AvpArena *a, const ShsmRepo *r, const char *mw_version, const char *exe);
/* Repo.asm_path(unit): build/<ver>/asm/<unit path>.s. */
const char *shsm_repo_asm_path(AvpArena *a, const ShsmRepo *r, const char *unit);
/* Repo.rel(path): os.path.relpath(path, root) with forward slashes (relpath's ValueError raised as SHSM_EXC_VALUE). */
const char *shsm_repo_rel(AvpArena *a, const ShsmRepo *r, const char *path);

/* repo.unit_path: 'main/game/PadRumble' or 'game\PadRumble.cpp' -> 'game/PadRumble' (backslashes to /, "main/"
 * dropped, extensions .c .cpp .cp .s .o .C .cc dropped). */
const char *shsm_unit_path(AvpArena *a, const char *unit);
/* repo.unit_name: 'main/' + unit_path(unit). */
const char *shsm_unit_name(AvpArena *a, const char *unit);

/* The main checkout of the repository root r belongs to: the parent of `git rev-parse --git-common-dir`. NULL when
 * git does not answer or the repository is bare. For the main checkout itself that is r->root. */
const char *shsm_repo_main_root(AvpArena *a, const ShsmRepo *r);
/* A reference input that lives only in the main checkout (orig/_ref, orig/_iso, build/binutils; rel is relative to
 * the root): <root>/rel when it exists, else <main checkout>/rel when that exists, else <root>/rel (the caller's
 * own "not found" message then names the lane path). A lane slot made before slot.exe linked those folders thus
 * still finds them. The first answer taken from the main checkout prints one `note:` line on stderr naming both
 * roots. Nothing is copied. */
const char *shsm_repo_shared_path(AvpArena *a, const ShsmRepo *r, const char *rel);
