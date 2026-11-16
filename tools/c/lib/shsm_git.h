/* git, once (what the Python tools each wrapped in their own `git()` helper: slot.py git(cwd, *args), merge_lane.py
 * Ctx.git, gate.py Gate.git, neutral.py git(root, *args), run_tests.py git(root, *argv), score_history.py Git,
 * hooks/pre_commit.py git()/blob_head(), doctor.py run()).
 *
 * A thin, typed layer over avp_run: git is started with an argument vector (no shell), so a path with spaces needs
 * no quoting. `checkout` is the folder git runs in (`git -C checkout ...`); NULL runs in this process's folder.
 * Nothing here writes to the repository except what the caller's own argument vector asks for (shsm_git_run).
 *
 * Conventions.
 *  - Strings come from the arena `a`; vectors (ShsmGit*Vec, AvpStrVec) are heap vectors (avp_vec_free optional in a
 *    short-lived tool). AvpStr fields are views into text owned by `a`.
 *  - Text mode is subprocess.run(..., text=True, encoding="utf-8", errors="replace"): invalid UTF-8 becomes U+FFFD,
 *    "\r\n" and a lone "\r" become "\n". Binary mode (ShsmGitOpts.binary, and every -z / blob call here) returns the
 *    bytes untouched. Lines are split on "\n" only (see lib/shsm_git.DEVIATIONS.md, D-shsm_git-2).
 *  - stdin is NUL unless ShsmGitOpts.input is given. Every call here that is a query sets core.quotepath=off, so
 *    non-ASCII names print raw.
 *  - Failure. A query whose Python form tested the return code and went on (rev-parse, merge-base, cat-file -e, ...)
 *    returns NULL / false. A query that fills an output vector takes a final `AvpStr *err`: NULL raises
 *    SHSM_EXC_USAGE "git <args> failed: <stderr>" (shsm_git_require); non-NULL returns false with the stripped
 *    stderr (else stdout) in *err and raises nothing. A git that cannot be started raises SHSM_EXC_USAGE "cannot run
 *    git: [WinError 2] ..." (ShsmGitOpts.soft_start returns instead). A timeout counts as a failure (rc -1).
 *  - Revisions and paths are passed as given; a name that starts with "-" is the caller's concern (use
 *    shsm_git_run with "--" for such). */
#pragma once
#include "avp/arena.h"
#include "avp/str.h"
#include "avp/vec.h"

/* ---- the runner ------------------------------------------------------------------------------------------------ */
typedef struct ShsmGitOpts {
    double timeout;                /* seconds; 0: none (the process ceiling of proc.h still applies) */
    const void *input;             /* stdin bytes when non-NULL (communicate(input=...)) */
    size_t input_n;
    const char *const *env;        /* NULL-terminated "NAME=VALUE" strings added to the inherited environment */
    bool quotepath_off;            /* `-c core.quotepath=off` */
    bool binary;                   /* raw bytes: no decoding, no newline translation */
    bool soft_start;               /* git cannot be started: return started == false, rc -1 instead of raising */
} ShsmGitOpts;

typedef struct ShsmGitRun {
    i64 rc;          /* the exit code as Python shows it (unsigned); -1 on a timeout or when git did not start */
    bool started;
    bool timed_out;
    AvpStr out, err; /* captured streams, NUL-terminated, never NULL */
} ShsmGitRun;

/* subprocess.run(["git", ("-c", "core.quotepath=off",) ("-C", checkout,) *args], capture_output=True, text=True,
 * ...) -> (rc, out, err). Never raises for a non-zero exit. opts may be NULL (all defaults). */
ShsmGitRun shsm_git_run(AvpArena *a, const char *checkout, const char *const *args, size_t n, const ShsmGitOpts *opts);
static inline bool shsm_git_ok(const ShsmGitRun *r) { return r->started && !r->timed_out && r->rc == 0; }
/* The text a failure message wants: (stderr or stdout).strip(), "timed out" for a timeout. */
AvpStr shsm_git_failure_text(const ShsmGitRun *r);
/* Raises SHSM_EXC_USAGE "git <args joined by spaces> failed: <failure text>" unless r is ok (slot.py git(...,
 * check=True) with the message of score_history.Git.run). */
void shsm_git_require(AvpArena *a, const ShsmGitRun *r, const char *const *args, size_t n);
/* shsm_git_run + shsm_git_require: the run when it succeeded. */
ShsmGitRun shsm_git_check(AvpArena *a, const char *checkout, const char *const *args, size_t n, const ShsmGitOpts *opts);
/* shutil.which("git") is not NULL. */
bool shsm_git_available(AvpArena *a);

/* Literal argument lists: SHSM_GIT(a, dir, "rev-parse", "HEAD"). */
#define SHSM_GIT_ARGS_(...) \
    (const char *const[]) { __VA_ARGS__ }, sizeof((const char *const[]) { __VA_ARGS__ }) / sizeof(const char *)
#define SHSM_GIT(a, checkout, ...) shsm_git_run((a), (checkout), SHSM_GIT_ARGS_(__VA_ARGS__), NULL)
#define SHSM_GIT_OPTS(a, checkout, opts, ...) shsm_git_run((a), (checkout), SHSM_GIT_ARGS_(__VA_ARGS__), (opts))
#define SHSM_GIT_CHECK(a, checkout, ...) shsm_git_check((a), (checkout), SHSM_GIT_ARGS_(__VA_ARGS__), NULL)

/* ---- splitting ------------------------------------------------------------------------------------------------- */
/* text.split("\n") without the empty piece after a final "\n" (views into text). */
AvpStrVec shsm_git_split_lines(AvpStr text);
/* data.split(b"\0") without the empty piece after a final NUL (empty fields in the middle are kept). */
AvpStrVec shsm_git_split_nul(AvpStr data);

/* ---- status ---------------------------------------------------------------------------------------------------- */
typedef struct ShsmGitStatusEntry {
    char xy[3];          /* the two status letters ("??" untracked, " M", "R ", "UU", ...), NUL-terminated */
    AvpStr path;         /* the path (the new name of a rename or copy) */
    AvpStr orig_path;    /* the old name when has_orig (X or Y is R or C) */
    bool has_orig;
} ShsmGitStatusEntry;
typedef AVP_VEC(ShsmGitStatusEntry) ShsmGitStatusVec;
/* `status --porcelain=v1 -z [--untracked-files=all]` (gate.py status; unit.py/draft.py `status --porcelain -uall`
 * with line[3:], without the quoting those lines needed). The -z records "XY path" and, for a rename or copy, the
 * old name as the next record. */
bool shsm_git_status(AvpArena *a, const char *checkout, bool untracked_all, ShsmGitStatusVec *out, AvpStr *err);
/* The parser alone: the bytes of `status --porcelain=v1 -z`. */
void shsm_git_parse_status(AvpStr z, ShsmGitStatusVec *out);
/* bool(git(path, "status", "--porcelain").strip()) (slot.is_dirty, merge_lane): true when anything differs. Raises
 * SHSM_EXC_USAGE "git status failed in <checkout>" when git fails (slot.is_dirty's ToolError). */
bool shsm_git_dirty(AvpArena *a, const char *checkout);

/* ---- rev-parse and refs ---------------------------------------------------------------------------------------- */
/* `rev-parse --verify -q REV^{commit}` -> the sha, or NULL (rc != 0). (score_history.Git.resolve, slot, merge_lane) */
char *shsm_git_rev_parse(AvpArena *a, const char *checkout, const char *rev);
/* `rev-parse --verify -q REV` (any object; "HEAD", a sha, "REV:PATH"), or NULL. */
char *shsm_git_rev_parse_object(AvpArena *a, const char *checkout, const char *rev);
/* `rev-parse --verify -q REV:PATH` -> the blob (or tree) id, NULL when absent (score_history.Git.rev_sha). */
char *shsm_git_object_id(AvpArena *a, const char *checkout, const char *rev, const char *path);
/* `rev-parse --abbrev-ref HEAD`: the branch, "HEAD" when detached, NULL when it fails (no commit yet). */
char *shsm_git_branch(AvpArena *a, const char *checkout);
/* `rev-parse --short HEAD`, or NULL. */
char *shsm_git_short_head(AvpArena *a, const char *checkout);
/* `rev-parse --verify --quiet REF` for a full ref name (rc == 0). */
bool shsm_git_ref_exists(AvpArena *a, const char *checkout, const char *ref);
/* shsm_git_ref_exists("refs/heads/" + branch) (slot.cmd_new). */
bool shsm_git_branch_exists(AvpArena *a, const char *checkout, const char *branch);
/* `rev-parse --show-toplevel --git-common-dir` in one call (slot.Repo.__init__). top is as git prints it (forward
 * slashes on Windows). common is absolute and normalised (backslashes, no "..") and resolved against the folder
 * git ran in; slot.py resolved it against the top-level, which differs only when checkout is a subfolder. False
 * when it is not a work tree. */
typedef struct ShsmGitRoots {
    char *top;
    char *common;
} ShsmGitRoots;
bool shsm_git_roots(AvpArena *a, const char *checkout, ShsmGitRoots *out);
/* `rev-parse --show-toplevel` or NULL (as git prints it; run_tests.py, worktree). */
char *shsm_git_toplevel(AvpArena *a, const char *checkout);
/* The absolute, normalised common directory (shsm_git_roots().common), or NULL. */
char *shsm_git_common_dir(AvpArena *a, const char *checkout);
/* `rev-parse --git-path NAME`, made absolute against the folder git ran in and normalised (merge_lane
 * op_in_progress, doctor.hooks_dir); NULL when git fails. */
char *shsm_git_git_path(AvpArena *a, const char *checkout, const char *name);

/* ---- ancestry -------------------------------------------------------------------------------------------------- */
/* `merge-base A B` -> the sha, or NULL (no common ancestor, unknown revision). */
char *shsm_git_merge_base(AvpArena *a, const char *checkout, const char *x, const char *y);
/* `merge-base --is-ancestor ANC DESC`: 0 when ANC is an ancestor of DESC (or equal), 1 when it is not, -1 on any
 * other failure (unknown revision, not a repository, timeout). slot.unmerged() tested only rc != 0, which counted a
 * failure as "not merged"; callers that need the difference test for -1. */
int shsm_git_is_ancestor(AvpArena *a, const char *checkout, const char *anc, const char *desc);
/* `rev-list --count RANGE` ("A..B"): true with *count. (slot, merge_lane) */
bool shsm_git_rev_count(AvpArena *a, const char *checkout, const char *range, i64 *count);

/* ---- diff ------------------------------------------------------------------------------------------------------ */
/* `diff --name-only -z ARGS...` -> the names. ARGS is any diff selection: {"REV"}, {"A", "B"}, {"A...B"},
 * {"--cached", "--diff-filter=ACMRT"} (merge_lane, slot, hooks/pre_commit.staged_entries). */
bool shsm_git_diff_names(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStrVec *out,
                         AvpStr *err);
/* `diff ARGS...` -> the text (`diff --stat`, patches). */
bool shsm_git_diff_text(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStr *out, AvpStr *err);
/* `diff-tree --no-commit-id --name-only -r -z SHA` -> the names the commit touched. */
bool shsm_git_diff_tree_names(AvpArena *a, const char *checkout, const char *sha, AvpStrVec *out, AvpStr *err);

/* ---- objects --------------------------------------------------------------------------------------------------- */
/* `cat-file blob SPEC` (SPEC "REV:PATH" or a blob sha): true with the bytes in *data (merge_lane, score_history
 * show_file, pre_commit); false when absent. `show REV:PATH` printed the same bytes. */
bool shsm_git_blob(AvpArena *a, const char *checkout, const char *spec, AvpStr *data);
/* shsm_git_blob("REV:PATH"). */
bool shsm_git_show_blob(AvpArena *a, const char *checkout, const char *rev, const char *path, AvpStr *data);
/* The first n bytes of the blob (pre_commit.blob_head: stops reading and kills git); false when absent. */
bool shsm_git_blob_head(AvpArena *a, const char *checkout, const char *spec, size_t n, AvpStr *data);
/* `cat-file -e SPEC` (merge_lane: does the path exist at ONTO). */
bool shsm_git_object_exists(AvpArena *a, const char *checkout, const char *spec);
/* `cat-file -s SPEC` -> the size in bytes. */
bool shsm_git_object_size(AvpArena *a, const char *checkout, const char *spec, i64 *size);
/* `cat-file --batch` over the object names: one entry per name, in order (score_history.Git.blobs). */
typedef struct ShsmGitObject {
    bool found;
    AvpStr type; /* "blob", "tree", "commit", "tag"; empty when not found */
    AvpStr data;
} ShsmGitObject;
typedef AVP_VEC(ShsmGitObject) ShsmGitObjectVec;
bool shsm_git_cat_batch(AvpArena *a, const char *checkout, const char *const *names, size_t n, ShsmGitObjectVec *out,
                        AvpStr *err);

/* ---- index and trees ------------------------------------------------------------------------------------------- */
/* `ls-files -z ARGS...` -> the names ({}, {"--others", "--exclude-standard"}, {"--cached", "--others",
 * "--exclude-standard"}, {"--", "dir"}; the Python callers with and without -z). */
bool shsm_git_ls_files(AvpArena *a, const char *checkout, const char *const *args, size_t n, AvpStrVec *out,
                       AvpStr *err);
/* `ls-files -s -z ARGS...`: "MODE SHA STAGE\tPATH". mode is the octal number (0100644, 0100755, 0120000,
 * SHSM_GIT_MODE_SUBMODULE), not Python's string. */
#define SHSM_GIT_MODE_SUBMODULE 0160000u
typedef struct ShsmGitStageEntry {
    u32 mode;
    int stage;
    AvpStr sha, path;
} ShsmGitStageEntry;
typedef AVP_VEC(ShsmGitStageEntry) ShsmGitStageVec;
bool shsm_git_ls_files_stage(AvpArena *a, const char *checkout, const char *const *args, size_t n,
                             ShsmGitStageVec *out, AvpStr *err);
/* `ls-tree -r -z REV [-- PATHS...]`: "MODE TYPE SHA\tPATH" (neutral.py, score_history.tree_files, which kept the
 * "blob" entries). */
typedef struct ShsmGitTreeEntry {
    u32 mode;
    AvpStr type, sha, path;
} ShsmGitTreeEntry;
typedef AVP_VEC(ShsmGitTreeEntry) ShsmGitTreeVec;
bool shsm_git_ls_tree(AvpArena *a, const char *checkout, const char *rev, const char *const *paths, size_t npaths,
                      ShsmGitTreeVec *out, AvpStr *err);
/* `check-ignore -z --stdin` over the paths: the ones git ignores, in input order (run_tests.py, which joined the
 * paths with newlines and split the answer on whitespace). Exit 1 (none ignored) is success with nothing. */
bool shsm_git_check_ignore(AvpArena *a, const char *checkout, const char *const *paths, size_t n, AvpStrVec *ignored,
                           AvpStr *err);

/* ---- worktrees ------------------------------------------------------------------------------------------------- */
typedef struct ShsmGitWorktree {
    char *path;            /* as git prints it ("E:/shsm-wt/slot3") */
    char *npath;           /* os.path.normpath(path): backslashes, for comparing with Windows paths */
    char *head;            /* the HEAD sha ("" for a bare entry) */
    char *branch;          /* the full ref "refs/heads/x", NULL when detached or bare */
    bool bare, detached, locked, prunable;
    char *locked_reason;   /* the text after "locked", or NULL */
    char *prunable_reason;
} ShsmGitWorktree;
typedef AVP_VEC(ShsmGitWorktree) ShsmGitWorktreeVec;
/* `worktree list --porcelain` (slot.Repo.worktrees, merge_lane, worktree). The first entry is the main work tree. */
bool shsm_git_worktrees(AvpArena *a, const char *checkout, ShsmGitWorktreeVec *out, AvpStr *err);
void shsm_git_parse_worktrees(AvpArena *a, AvpStr text, ShsmGitWorktreeVec *out);
/* branch with "refs/heads/" removed once (slot: .replace("refs/heads/", "", 1)); NULL stays NULL. */
const char *shsm_git_branch_short(const char *ref);
/* slot.norm(): the comparable form of a path: absolute, normalised separators and case, no \\?\ or \??\ prefix. */
char *shsm_git_path_key(AvpArena *a, const char *path);

/* ---- merge-tree ------------------------------------------------------------------------------------------------ */
typedef struct ShsmGitMergeTree {
    bool clean;
    char *tree;               /* the (possibly conflicted) result tree id */
    AvpStrVec conflicts;      /* conflicted file names (empty when clean) */
} ShsmGitMergeTree;
/* `merge-tree --write-tree --name-only --no-messages -z A B` (slot.cmd_conflicts): exit 0 clean, 1 conflicts, any
 * other exit is a failure. Writes tree objects into the object database (as the Python did), changes no work tree. */
bool shsm_git_merge_tree(AvpArena *a, const char *checkout, const char *x, const char *y, ShsmGitMergeTree *out,
                         AvpStr *err);

/* ---- log ------------------------------------------------------------------------------------------------------- */
/* `log -1 --format=%H -- PATH`: the newest commit touching PATH, or NULL when none (or it fails). */
char *shsm_git_last_commit_for_path(AvpArena *a, const char *checkout, const char *path);
/* `log --format=%H --reverse [--first-parent] -- PATH`: oldest first (score_history.Git.baseline_revs). */
bool shsm_git_log_path_shas(AvpArena *a, const char *checkout, const char *path, bool first_parent, AvpStrVec *out,
                            AvpStr *err);
/* `log -1 --format=%s REV`, stripped; NULL when it fails. */
char *shsm_git_subject(AvpArena *a, const char *checkout, const char *rev);
/* `show -s --format=%ct%x09%s REV` -> the commit time (seconds) and subject (score_history.Git.commit_info). */
bool shsm_git_commit_info(AvpArena *a, const char *checkout, const char *rev, i64 *commit_time, char **subject);
/* `branch --no-merged ONTO --format=%(refname:short)` -> the branches (slot.cmd_conflicts). */
bool shsm_git_branches_no_merged(AvpArena *a, const char *checkout, const char *onto, AvpStrVec *out, AvpStr *err);
