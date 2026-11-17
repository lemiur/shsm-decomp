/* wdbg_run: one compile under the debugger (wdbg_run.h). The order of the single-run path of wdbg.exe is kept: the
 * process is created, then the log opened, then the spec parsed and loaded, then the event loop runs. */
#include "wdbg_run.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* A job whose last handle, when closed, kills every process in it. NULL when the system refuses one (the run then
 * goes on without it: the debug API's kill-on-exit still ends the compiler with this process). */
static HANDLE kill_on_close_job(void) {
    HANDLE job = CreateJobObjectW(NULL, NULL);
    if (!job) return NULL;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li;
    memset(&li, 0, sizeof li);
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li)) {
        CloseHandle(job);
        return NULL;
    }
    return job;
}

bool wdbg_run(AvpArena *a, const WdbgRun *r, const char **cls, const char **msg, i64 *exit_code) {
    *cls = NULL;
    *msg = NULL;
    *exit_code = -1;
    const char *cmdline = avp_list2cmdline_n(a, r->argv, r->argc);
    STARTUPINFOW si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof pi);
    wchar_t *wcmd = avp_to_w(a, avp_str(cmdline));
    wchar_t *wcwd = avp_to_w(a, avp_str(r->cwd));
    HANDLE job = kill_on_close_job();
    /* output_path: the compiler's stdout and stderr go to that file, and only that handle is inherited */
    HANDLE outh = INVALID_HANDLE_VALUE;
    STARTUPINFOEXW sx;
    memset(&sx, 0, sizeof sx);
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = NULL;
    BOOL inherit = FALSE;
    DWORD extra = 0;
    if (r->output_path) {
        SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
        outh = CreateFileW(avp_to_w(a, avp_str(r->output_path)), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        SIZE_T sz = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &sz);
        attrs = outh != INVALID_HANDLE_VALUE ? avp_alloc(a, sz) : NULL;
        if (attrs && InitializeProcThreadAttributeList(attrs, 1, 0, &sz) &&
            UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &outh, sizeof outh, NULL, NULL)) {
            sx.StartupInfo.cb = sizeof sx;
            sx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
            sx.StartupInfo.hStdInput = NULL;
            sx.StartupInfo.hStdOutput = outh;
            sx.StartupInfo.hStdError = outh;
            sx.lpAttributeList = attrs;
            inherit = TRUE;
            extra = EXTENDED_STARTUPINFO_PRESENT;
        } else {
            attrs = NULL;
        }
    }
    /* DEBUG_PROCESS, handles not inherited (but the output file's), as wdbg.py's CreateProcessW; suspended until it is
     * in the job */
    BOOL created = CreateProcessW(NULL, wcmd, NULL, NULL, inherit, DEBUG_PROCESS | CREATE_SUSPENDED | extra, NULL, wcwd,
                                  inherit ? &sx.StartupInfo : &si, &pi);
    if (attrs) DeleteProcThreadAttributeList(attrs);
    if (outh != INVALID_HANDLE_VALUE) CloseHandle(outh);
    if (!created) {
        *cls = "AssertionError";
        *msg = avp_sprintf(a, "%lu", (unsigned long)GetLastError());
        if (job) CloseHandle(job);
        return false;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);

    WdbgDebug d = {0};
    d.process = pi.hProcess;
    d.outdir = r->outdir;
    d.exe = r->exe;
    d.build = r->build;
    bool ok = false;
    /* open(outdir+'/log.txt','w'): the locale encoding (the ANSI code page), "\n" written as "\r\n" */
    AvpFile *log = avp_fopen(avp_sprintf(a, "%s/log.txt", r->outdir), "w", AVP_TEXT(AVP_ENC_MBCS, AVP_ERR_STRICT, AVP_NL_DEFAULT));
    if (!log) {
        *cls = avp_io_err_class();
        *msg = avp_cz(a, avp_io_err_str(a));
        goto done;
    }
    d.log = log;
    wdbg_debug_out(&d, avp_str(avp_sprintf(a, "compiler: %s (tools/compilers/%s/mwcceppc.exe)", d.build, d.build)));
    const char *err = NULL;
    WdbgSpec *spec = wdbg_spec_parse(a, r->spec_path, r->spec_text, &err);
    if (!spec) {
        *cls = "SyntaxError";
        *msg = err;
        goto done;
    }
    WdbgHost host = wdbg_debug_host(&d);
    if (!wdbg_spec_load(spec, &host, &err)) { /* err is "Class: message" */
        *msg = err;
        goto done;
    }
    wdbg_debug_loop(&d, spec);
    if (d.exited) *exit_code = (i64)d.exit_code;
    ok = true;
done:
    if (log) avp_fclose(log);
    if (job) CloseHandle(job); /* kills the compiler if it is still running (a timeout, a load error) */
    else if (!ok) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok;
}
