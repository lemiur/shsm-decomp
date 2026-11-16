/* permuter_run: the launcher pieces (permuter_run.h). */
#include "permuter_run.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* tools/mwcc_trace/permuter_run.py, statement for statement, as a `python -c` program. Under the script sys.path[0]
 * is the script's directory and argparse's prog the script's name; both are set to the same here. */
const char PERMUTER_RUN_BOOTSTRAP[] =
    "import os, sys\n"
    "root = sys.argv.pop(1)\n"
    "sys.argv[0] = 'permuter_run.py'\n"
    "sys.path[0] = os.path.join(root, 'tools', 'mwcc_trace')\n"
    "PERM = os.path.join(root, 'tools', 'permuter')\n"
    "sys.path.insert(0, PERM)\n"
    "import src.main\n"
    "def _read_preprocessed(filename, cpp_args=[]):\n"
    "    with open(filename, encoding='utf-8') as f:\n"
    "        return f.read()\n"
    "src.main.preprocess = _read_preprocessed\n"
    "src.main.main()\n";

const char *permuter_run_python(AvpArena *a, const char *root) {
    return avp_join(a, root, "tools", "venv", "Scripts", "python.exe");
}

const char **permuter_run_argv(AvpArena *a, const char *python, const char *root, const char *const *args, size_t n,
                               size_t *argc_out) {
    const char **v = AVP_NEWN(a, const char *, n + 5);
    size_t k = 0;
    v[k++] = python;
    v[k++] = "-c";
    v[k++] = PERMUTER_RUN_BOOTSTRAP;
    v[k++] = root;
    for (size_t i = 0; i < n; i++) v[k++] = args[i];
    v[k] = NULL;
    *argc_out = k;
    return v;
}

/* Ctrl+C and Ctrl+Break reach every process of the console: the child (the permuter) handles them, this process only
 * waits for it. A handler routine, not SetConsoleCtrlHandler(NULL, TRUE), whose "ignore" flag children inherit. */
static BOOL WINAPI on_ctrl(DWORD type) { return type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT; }

static HANDLE inheritable(DWORD which) {
    HANDLE h = GetStdHandle(which), dup = NULL;
    if (h == NULL || h == INVALID_HANDLE_VALUE) return h;
    if (DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) return dup;
    return h;
}

i64 permuter_run_spawn(AvpArena *a, const char *const *argv, size_t argc, const char **err) {
    wchar_t *cmd = avp_to_w(a, avp_str(avp_list2cmdline_n(a, argv, argc)));
    STARTUPINFOW si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inheritable(STD_INPUT_HANDLE);
    si.hStdOutput = inheritable(STD_OUTPUT_HANDLE);
    si.hStdError = inheritable(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);
    fflush(stdout);
    fflush(stderr);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        *err = avp_cz(a, avp_win_error_str(a, GetLastError()));
        return -1;
    }
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
        memset(&li, 0, sizeof li);
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
        AssignProcessToJobObject(job, pi.hProcess);
    }
    SetConsoleCtrlHandler(on_ctrl, TRUE);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    SetConsoleCtrlHandler(on_ctrl, FALSE);
    return (i64)code;
}
