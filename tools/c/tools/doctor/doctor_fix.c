/* doctor: --fix (doctor.Fixer) and --install-hooks / --uninstall-hooks (install_hooks, uninstall_hooks). */
#include "doctor.h"
#include "shsm_cli.h"

#include <stdio.h>
#include <string.h>

#define A avp_arena()
#define FMT(...) ((char *)avp_pyfmt(A, __VA_ARGS__).p)

/* print(msg, file=out, flush=True) */
static void say(AvpBuf *out, const char *msg) {
    if (out) {
        avp_buf_putz(out, msg);
        avp_buf_putc(out, '\n');
        return;
    }
    fputs(msg, stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

/* subprocess.run(cmd, cwd=cwd).returncode: the standard handles are inherited. A program that cannot start raises the
 * OSError Python's subprocess.run raised (main prints it and exits 2). */
i64 doctor_fix_run(void *ctx, const char *const *argv, size_t argc, const char *cwd) {
    (void)ctx;
    const char **v = AVP_NEWN(A, const char *, argc + 1);
    for (size_t i = 0; i < argc; i++) v[i] = argv[i];
    AvpProcResult r = avp_run(&(AvpProcSpec){.argv = v, .cwd = cwd, .arena = A});
    if (r.status == AVP_PROC_START_FAILED)
        shsm_raise(r.winerror == 2 || r.winerror == 3 ? SHSM_EXC_FILE_NOT_FOUND : SHSM_EXC_OS, "%s",
                   r.message ? r.message : "cannot start");
    return r.returncode;
}

static bool step(DoctorFixer *f, const char *const *cmd, size_t n) {
    say(f->out, FMT("$ %s", avp_list2cmdline_n(A, cmd, n)));
    i64 rc = f->runner(f->runner_ctx, cmd, n, f->d->root);
    if (rc != 0) say(f->out, FMT("FAILED (exit %d): stopped", rc));
    return rc == 0;
}
#define STEP(f, ...) step((f), (const char *const[]){__VA_ARGS__}, sizeof((const char *const[]){__VA_ARGS__}) / sizeof(char *))

static bool fix_venv(DoctorFixer *f) {
    const char *root = f->d->root;
    char *venv = avp_join(A, root, "tools", "venv");
    if (!doctor_venv_python(A, root)) {
        if (avp_exists(venv)) {
            say(f->out, "venv: tools/venv exists without a python; move it away first");
            return false;
        }
        const char *sys_py = avp_which(A, "python", NULL, NULL); /* only to create the venv */
        if (!sys_py) {
            say(f->out, "venv: no python on PATH to create tools/venv");
            return false;
        }
        if (!STEP(f, sys_py, "-m", "venv", avp_join(A, "tools", "venv"))) return false;
    }
    char *py = doctor_venv_python(A, root);
    if (!py) py = avp_join(A, venv, "Scripts", "python.exe");
    if (strcmp(doctor_check_venv(f->d).status, "OK") == 0) {
        say(f->out, "venv: ok");
        return true;
    }
    const char *const *pk = doctor_pin("venv")->packages;
    return STEP(f, py, "-m", "pip", "install", pk[0], pk[1], pk[2]);
}

static bool fix_permuter(DoctorFixer *f) {
    const DoctorPin *pin = doctor_pin("permuter");
    char *dest = avp_join(A, "tools", "permuter");
    char *full = avp_path_join(A, f->d->root, dest);
    if (avp_exists(full)) {
        if (!avp_isdir(avp_path_join(A, full, ".git"))) {
            say(f->out, "permuter: tools/permuter exists and is not a git checkout; move it away first");
            return false;
        }
        if (strcmp(doctor_check_permuter(f->d).status, "OK") == 0) {
            say(f->out, "permuter: ok");
            return true;
        }
        if (!STEP(f, "git", "-C", dest, "fetch", "origin")) return false;
    } else if (!STEP(f, "git", "clone", pin->url, dest)) {
        return false;
    }
    return STEP(f, "git", "-C", dest, "checkout", "--detach", pin->commit);
}

static bool fix_binutils(DoctorFixer *f) {
    const DoctorPin *pin = doctor_pin("binutils");
    char *tag = doctor_configure_binutils_tag(A, f->d->root);
    if (!tag || strcmp(tag, pin->version) != 0) {
        say(f->out, FMT("binutils: configure.py binutils_tag %s is not the pinned %s; not downloading",
                        tag ? avp_repr_str(A, avp_str(tag)).p : "None", pin->version));
        return false;
    }
    if (avp_isdir(avp_path_join(A, f->d->root, pin->path))) {
        say(f->out, "binutils: build/binutils exists");
        return true;
    }
    /* download_tool.exe beside doctor.exe: `download_tool [-h] --tag TAG tool output` */
    return STEP(f, avp_path_join(A, avp_exe_dir(), "download_tool.exe"), "binutils", pin->path, "--tag", tag);
}

bool doctor_fix(DoctorFixer *f, const char *const *components, size_t n) {
    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        const char *comp = components[i];
        say(f->out, FMT("== %s", comp));
        bool r = !strcmp(comp, "venv") ? fix_venv(f) : !strcmp(comp, "permuter") ? fix_permuter(f) : fix_binutils(f);
        ok = r && ok;
    }
    return ok;
}

/* ---- hooks ----------------------------------------------------------------------------------------------------- */
static bool has_marker(const char *path) { return doctor_hook_shim(path) != NULL; }

const char *doctor_hook_shim(const char *path) {
    AvpStr t;
    if (!avp_read_text(A, path, AVP_TM_UTF8_REPLACE, &t)) shsm_raise_io();
    if (!avp_str_contains(t, AVP_S(DOCTOR_HOOK_MARKER))) return NULL;
    return avp_str_contains(t, AVP_S("build/tools/pre_commit.exe")) ? "pre-commit-c" : "pre-commit";
}

const char *doctor_hook_source(const char *root) {
    char *c = avp_join(A, root, "tools", "hooks", "pre-commit-c");
    if (avp_isfile(c)) return c;
    return NULL;
}

int doctor_install_hooks(const char *root, AvpBuf *out) {
    char *hd = doctor_hooks_dir(A, root);
    const char *src = doctor_hook_source(root);
    if (!hd || !src) {
        say(out, "error: hooks directory or tools/hooks/pre-commit-c not found");
        return 2;
    }
    char *dst = avp_path_join(A, hd, "pre-commit");
    if (avp_isfile(dst) && !has_marker(dst)) {
        say(out, FMT("REFUSED: %s exists and was not installed by doctor.exe", dst));
        return 1;
    }
    if (!avp_makedirs(hd, true)) shsm_raise_io();
    AvpBytes data = {0};
    if (!avp_read_all(A, src, &data)) shsm_raise_io();
    /* a CRLF checkout would break the #!/bin/sh line */
    AvpStr lf = avp_str_replace(A, avp_strn((const char *)data.v, data.n), AVP_S("\r\n"), AVP_S("\n"), -1);
    if (!avp_write_file(dst, lf.p, lf.n)) shsm_raise_io();
    if (!avp_chmod(dst, 0755)) shsm_raise_io();
    say(out, FMT("installed %s", dst));
    return 0;
}

int doctor_uninstall_hooks(const char *root, AvpBuf *out) {
    char *hd = doctor_hooks_dir(A, root);
    char *dst = hd ? avp_path_join(A, hd, "pre-commit") : "";
    if (!avp_isfile(dst)) {
        say(out, "no pre-commit hook installed");
        return 0;
    }
    if (!has_marker(dst)) {
        say(out, FMT("REFUSED: %s was not installed by doctor.exe", dst));
        return 1;
    }
    if (!avp_remove(dst)) shsm_raise_io();
    say(out, FMT("removed %s", dst));
    return 0;
}
