/* doctor: the environment self-check (a port of python tools/doctor.py). One DoctorCheck per line; --fix restores
 * the optional components through an injectable command runner; --install-hooks / --uninstall-hooks copy or remove
 * the pre-commit shim. The checks that ask a command (ninja --version, decomp.exe status, the asm index) go through
 * Doctor.runner so tests answer them; the venv, permuter and hooks-directory probes run directly, as in the Python. */
#pragma once
#include "avp/avp.h"

/* ---- pins (doctor.PINS, values as written in docs/toolchain.md) ------------------------------------------- */
typedef struct DoctorPin {
    const char *key;                /* "dtk", "objdiff-cli", ... */
    const char *path;
    const char *version;            /* NULL: none */
    const char *url;
    const char *sha256, *zip_sha256, *sha1;
    const char *asset;
    const char *commit;
    const char *const *packages;    /* NULL-terminated, venv only */
} DoctorPin;
extern const DoctorPin doctor_pins[];
extern const size_t doctor_npins;
const DoctorPin *doctor_pin(const char *key); /* PINS[key]; NULL when unknown */

extern const char *const doctor_compilers_required[]; /* NULL-terminated */
extern const char *const doctor_compilers_optional[];
extern const char *const doctor_build_root_ok[];
extern const char *const doctor_fix_components[];     /* "venv", "permuter", "binutils" */
#define DOCTOR_HOOK_MARKER "# shsm-hook:"

/* ---- checks ---------------------------------------------------------------------------------------------------- */
typedef struct DoctorCheck {
    const char *name, *status, *detail, *fix;
    bool required, advisory;
} DoctorCheck;
typedef AVP_VEC(DoctorCheck) DoctorChecks;

/* Check.line(): "%-8s name: detail" plus "  [fix: ...]" and " (optional)"/" (advisory)" when not OK. */
char *doctor_check_line(AvpArena *a, const DoctorCheck *c);
AvpJson *doctor_check_json(AvpArena *a, const DoctorCheck *c); /* Check.as_dict() */

/* run(cmd, cwd=None, timeout=60) -> (returncode, stripped stdout): -1 and str(e) when it cannot start or times out. */
typedef struct DoctorRunResult {
    i64 rc;
    const char *out;
} DoctorRunResult;
typedef DoctorRunResult (*DoctorRunner)(void *ctx, const char *const *argv, size_t argc, const char *cwd);
DoctorRunResult doctor_run(void *ctx, const char *const *argv, size_t argc, const char *cwd); /* the real one */

typedef struct Doctor {
    const char *root;        /* absolute checkout root */
    const char *dol_sha1;    /* PINS["dol"]["sha1"] (tests replace it) */
    DoctorRunner runner;     /* the injectable runner of check_required / check_ghidra / check_index */
    void *runner_ctx;
    const char *asmindex;    /* asmindex.exe beside doctor.exe (the asm-index line runs it) */
    const char *decomp;      /* decomp.exe beside doctor.exe (the ghidra line runs its status) */
} Doctor;
/* A Doctor for root with the real runner and pins. */
Doctor doctor_new(const char *root);


void doctor_check_required(Doctor *d, DoctorChecks *out);   /* check_required(root, runner) */
void doctor_check_all(Doctor *d, DoctorChecks *out);        /* check_all(root) */
DoctorCheck doctor_check_ghidra(Doctor *d);
DoctorCheck doctor_check_index(Doctor *d);
DoctorCheck doctor_check_asm_listings(Doctor *d);
DoctorCheck doctor_check_venv(Doctor *d);
DoctorCheck doctor_check_permuter(Doctor *d);
void doctor_check_advisory(Doctor *d, DoctorChecks *out);
void doctor_check_slots(Doctor *d, DoctorChecks *out);
const DoctorCheck *doctor_find(const DoctorChecks *v, const char *name); /* by name (the tests' dict), NULL if none */

/* Helpers the checks share (exposed for tests). */
char *doctor_configure_binutils_tag(AvpArena *a, const char *root); /* NULL when absent */
char *doctor_venv_python(AvpArena *a, const char *root);
char *doctor_hooks_dir(AvpArena *a, const char *root);               /* NULL when git fails */
/* The ghidra line's regex `(\S+) \(.*?\) at (.+?)(?:, language .*)?$` on g: true with the version and install. */
bool doctor_parse_ghidra(AvpStr g, AvpStr *version, AvpStr *install);
/* text[:n] / text[-n:] in code points. */
AvpStr doctor_cp_head(AvpStr s, size_t n);
AvpStr doctor_cp_tail(AvpStr s, size_t n);

/* ---- --fix ----------------------------------------------------------------------------------------------------- */
/* runner(cmd, cwd) -> returncode (subprocess.run(cmd, cwd=cwd).returncode: inherited standard handles). */
typedef i64 (*DoctorFixRunner)(void *ctx, const char *const *argv, size_t argc, const char *cwd);
i64 doctor_fix_run(void *ctx, const char *const *argv, size_t argc, const char *cwd); /* the real one */
typedef struct DoctorFixer {
    Doctor *d;
    DoctorFixRunner runner;
    void *runner_ctx;
    AvpBuf *out;             /* NULL: stdout (flushed after each line, as print(..., flush=True)) */
} DoctorFixer;
bool doctor_fix(DoctorFixer *f, const char *const *components, size_t n);

/* ---- hooks: return the exit code; messages go to out (NULL: stdout) --------------------------------------------
 * --install-hooks copies tools/hooks/pre-commit-c (the shim that runs build/tools/pre_commit.exe) into the hooks
 * directory, LF only (DEVIATIONS.md D-doctor-7). A hook carrying the DOCTOR_HOOK_MARKER line counts as installed. */
const char *doctor_hook_source(const char *root); /* the shim --install-hooks copies; NULL when there is none */
/* "pre-commit-c" or "pre-commit" for a hook file carrying the marker (the C shim, or an older Python shim), NULL for a
 * foreign hook; raises on an unreadable file. */
const char *doctor_hook_shim(const char *path);
int doctor_install_hooks(const char *root, AvpBuf *out);
int doctor_uninstall_hooks(const char *root, AvpBuf *out);

/* ---- the command line ------------------------------------------------------------------------------------------ */
int doctor_main(int argc, char **argv);
