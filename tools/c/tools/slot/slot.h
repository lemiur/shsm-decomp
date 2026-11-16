/* slot: the worktree slot manager (a port of tools/slot.py): create, check, lock, reset and safely remove the lane
 * worktrees ("slots"). Same stdout, exit codes, stderr class and lock files as the Python, so both tools honour each
 * other's locks. Accepted differences: DEVIATIONS.md. */
#pragma once
#include "avp/avp.h"
#include "shsm_cli.h"
#include "shsm_git.h"
#include "shsm_links.h"

/* The exceptions of slot.py, as avp_throw codes: Refusal (exit 1, "REFUSED: ..." on stdout), ToolError (exit 2,
 * "error: ..." on stderr) and an exception the Python did not catch (a traceback, exit 1). The full message is
 * slot_error_message() (AvpTry.msg is capped). */
enum { SLOT_REFUSAL = 300, SLOT_TOOL_ERROR, SLOT_UNCAUGHT };
AVP_NORETURN void slot_fail(int code, AVP_PRINTF_FMT const char *fmt, ...);
const char *slot_error_message(void);

/* slot.SHARED: the gitignored inputs a slot shares with the main checkout, with list's abbreviation. */
typedef struct {
    const char *rel;
    bool required;
    const char *abbr;
    bool copy_ok; /* a real directory in the slot is fine (state "local"), as for a downloaded tool set */
} SlotShared;
#define SLOT_NSHARED 10
extern const SlotShared SLOT_SHARED[SLOT_NSHARED];
#define SLOT_NREQUIRED 7
extern const char *const SLOT_REQUIRED_FILES[SLOT_NREQUIRED];
#define SLOT_MAIN_REF "main"
#define SLOT_LOCK_DIR "slot-locks"
#define SLOT_DEFAULT_TTL_HOURS 12.0

/* One `git worktree list --porcelain` entry (Repo.worktrees), or a bare path record (unknown: git does not know it). */
typedef struct {
    const char *path;   /* os.path.normpath of git's path */
    const char *head;   /* "" when none */
    const char *branch; /* short name, NULL when detached */
    bool locked, prunable, unknown;
} SlotTree;

typedef struct {
    const char *cwd;    /* the top-level, as git prints it */
    const char *common; /* slot.py: norm(join(top, git-common-dir)) */
    const char *base;
    const char *main;   /* trees[0].path */
    SlotTree *trees;
    size_t n;
} SlotRepo;

/* ---- paths and links (slot.norm, same, is_link, link_target, remove_link, scan_links) ---- */
const char *slot_norm(AvpArena *a, const char *path);
bool slot_same(AvpArena *a, const char *x, const char *y);
bool slot_is_link(const char *path);
const char *slot_link_target(AvpArena *a, const char *path);
/* Removes the link itself; a path that is not a link is a Refusal "not a link: <path>"; an OS failure raises it. */
void slot_remove_link(const char *path);
/* Every reparse point under root (root itself when it is one), absolute, in sorted() order. */
AvpStrVec slot_scan_links(AvpArena *a, const char *root);

/* ---- repository ---- */
SlotRepo *slot_repo_open(AvpArena *a, const char *repo, const char *base);
SlotTree slot_repo_find(AvpArena *a, const SlotRepo *r, const char *arg);
const char *slot_lock_path(AvpArena *a, const SlotRepo *r, const char *path);

/* ---- health ---- */
typedef struct {
    const char *rel;
    bool required;
    const char *state; /* ok, missing, copy, local, wrong, broken, n/a */
    const char *detail;
} SlotLinkState;
typedef struct {
    SlotLinkState links[SLOT_NSHARED];
    AvpStrVec problems, missing_files, extra_links;
} SlotHealth;
void slot_link_states(AvpArena *a, const char *slot, const char *main, SlotLinkState out[SLOT_NSHARED]);
SlotHealth slot_health(AvpArena *a, const char *slot, const char *main);

/* ---- locks: the JSON object of the lock file plus "file" (and Python's corrupt record), or NULL ---- */
AvpJson *slot_read_lock(AvpArena *a, const SlotRepo *r, const char *path);
double slot_lock_age_h(const AvpJson *lock, double now);
bool slot_lock_is_stale(const AvpJson *lock, double now);
const char *slot_lock_text(AvpArena *a, const AvpJson *lock, double now);
/* lock.get("owner") == owner (owner NULL is None). */
bool slot_lock_owner_is(const AvpJson *lock, const char *owner);
/* str() of a JSON value as Python prints it ("None" for NULL). */
const char *slot_py_str(AvpArena *a, const AvpJson *v);

/* ---- commands: the parsed arguments, then the exit code ---- */
typedef struct {
    const char *slot, *from_rev, *branch, *to, *owner, *lane, *onto;
    double ttl;
    bool force;
    bool fix; /* check --fix: create the missing links to the main checkout's shared inputs first */
    AvpStrVec branches;
} SlotArgs;
int slot_cmd_list(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_new(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_check(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_link(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_reset(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_remove(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_lock(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_unlock(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_who(AvpArena *a, SlotRepo *r, const SlotArgs *args);
int slot_cmd_conflicts(AvpArena *a, SlotRepo *r, const SlotArgs *args);

/* slot.main(argv): argv[0] is the program. */
int slot_main(int argc, char **argv);
