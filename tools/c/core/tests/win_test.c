/* win.h: registry (winreg), named mutexes, byte-range file locks (msvcrt.locking), processes, volumes, console,
 * undecoration (dbghelp), AES (bcrypt) and Win32 error text. Expected values noted beside the checks come from CPython
 * 3.14 (winreg, ctypes, os, socket) on a Windows machine. */
#include "avp/avp.h"
#include "avp/fs.h"
#include "avp/test.h"
#include "avp/win.h"
#include "avp/proc.h"
#include "avp/pool.h"

#include <stdio.h>
#include <windows.h>

static bool ieq(const char *a, const char *b) { return _stricmp(a, b) == 0; }

AVP_TEST(error_messages) {
    AvpArena *a = avp_arena_new(0);
    /* FormatMessage text with the trailing period removed, as OSError shows it */
    CHECK_CZ(avp_win_error_message(a, 2).p, "The system cannot find the file specified");
    CHECK_CZ(avp_win_error_message(a, 3).p, "The system cannot find the path specified");
    CHECK_CZ(avp_win_error_message(a, 5).p, "Access is denied");
    CHECK_CZ(avp_win_error_message(a, 183).p, "Cannot create a file when that file already exists");
    CHECK_CZ(avp_win_error_message(a, 145).p, "The directory is not empty");
    CHECK_CZ(avp_win_error_str(a, 5).p, "[WinError 5] Access is denied");
    CHECK_CZ(avp_win_error_message(a, 0xFFFFF).p, "Windows Error 0xfffff");
    /* errmap.h: OSError(0, "x", None, w).errno */
    CHECK_EQ_I64(avp_winerror_to_errno(2), ENOENT);
    CHECK_EQ_I64(avp_winerror_to_errno(3), ENOENT);
    CHECK_EQ_I64(avp_winerror_to_errno(5), EACCES);
    CHECK_EQ_I64(avp_winerror_to_errno(32), EACCES);
    CHECK_EQ_I64(avp_winerror_to_errno(80), EEXIST);
    CHECK_EQ_I64(avp_winerror_to_errno(183), EEXIST);
    CHECK_EQ_I64(avp_winerror_to_errno(145), ENOTEMPTY);
    CHECK_EQ_I64(avp_winerror_to_errno(267), ENOTDIR);
    CHECK_EQ_I64(avp_winerror_to_errno(112), ENOSPC);
    CHECK_EQ_I64(avp_winerror_to_errno(87), EINVAL);
    CHECK_EQ_I64(avp_winerror_to_errno(123456), EINVAL);
    CHECK_EQ_I64(avp_winerror_to_errno(1816), ENOMEM);
    avp_arena_free(a);
}

AVP_TEST(registry_reads) {
    AvpArena *a = avp_arena_new(0);
    /* winreg.QueryValueEx on HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion: ProgramFilesDir is
     * 'C:\Program Files' in the 64-bit view and 'C:\Program Files (x86)' in the 32-bit view */
    AvpRegValue v;
    CHECK(avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion", AVP_KEY_WOW64_64KEY, "ProgramFilesDir", &v));
    CHECK_EQ_I64(v.type, AVP_REG_SZ);
    CHECK(ieq(v.str.p, "C:\\Program Files"));
    CHECK(avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion", AVP_KEY_WOW64_32KEY, "ProgramFilesDir", &v));
    CHECK(ieq(v.str.p, "C:\\Program Files (x86)"));
    AvpStr s;
    CHECK(avp_reg_get_string(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, "SystemRoot", &s));
    char *sysroot = avp_getenv(a, "SYSTEMROOT");
    CHECK(sysroot && ieq(s.p, sysroot));
    /* REG_DWORD: CurrentMajorVersionNumber (10) */
    CHECK(avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, "CurrentMajorVersionNumber", &v));
    CHECK_EQ_I64(v.type, AVP_REG_DWORD);
    CHECK(v.num >= 10);
    /* a string value asked as a string only: a DWORD gives false without an error */
    CHECK(!avp_reg_get_string(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, "CurrentMajorVersionNumber", &s));
    /* REG_MULTI_SZ: BootExecute is ['autocheck autochk *'] on a stock machine */
    if (avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager", 0, "BootExecute", &v)) {
        CHECK_EQ_I64(v.type, AVP_REG_MULTI_SZ);
        CHECK(v.multi.n >= 1);
    }
    /* the default value of a key (name "") and the missing cases:
     * OSError [WinError 2] The system cannot find the file specified (FileNotFoundError), no filename */
    CHECK(!avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\NoSuchKeyZ", 0, "x", &v));
    CHECK_CZ(avp_io_err_class(), "FileNotFoundError");
    CHECK_CZ(avp_io_err_str(a).p, "[WinError 2] The system cannot find the file specified");
    CHECK(!avp_reg_get(a, AVP_HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, "NoValueZ", &v));
    CHECK_CZ(avp_io_err_str(a).p, "[WinError 2] The system cannot find the file specified");
    avp_arena_free(a);
}

typedef struct {
    int result;
    const char *name;
} MutexArg;

static DWORD WINAPI mutex_thread(void *p) {
    MutexArg *m = (MutexArg *)p;
    AvpNamedMutex *mx = avp_named_mutex_open(m->name);
    m->result = mx ? avp_named_mutex_wait(mx, 100) : -1;
    if (mx && m->result == AVP_WAIT_ACQUIRED) avp_named_mutex_release(mx);
    avp_named_mutex_close(mx);
    return 0;
}

AVP_TEST(named_mutex) {
    char name[64];
    snprintf(name, sizeof name, "AvpFsTestMutex%u", avp_getpid());
    AvpNamedMutex *m = avp_named_mutex_open(name);
    REQUIRE(m != NULL);
    CHECK_EQ_I64(avp_named_mutex_wait(m, 1000), AVP_WAIT_ACQUIRED);
    char full[96];
    snprintf(full, sizeof full, "Global\\%s", name);
    char local[96];
    snprintf(local, sizeof local, "Local\\%s", name);
    CHECK(avp_named_mutex_exists(full) || avp_named_mutex_exists(local));
    /* another thread (its own handle) cannot take it while we hold it */
    MutexArg arg = {-2, name};
    HANDLE t = CreateThread(NULL, 0, mutex_thread, &arg, 0, NULL);
    WaitForSingleObject(t, 5000);
    CloseHandle(t);
    CHECK_EQ_I64(arg.result, AVP_WAIT_TIMEOUT);
    CHECK(avp_named_mutex_release(m));
    arg.result = -2;
    t = CreateThread(NULL, 0, mutex_thread, &arg, 0, NULL);
    WaitForSingleObject(t, 5000);
    CloseHandle(t);
    CHECK_EQ_I64(arg.result, AVP_WAIT_ACQUIRED);
    avp_named_mutex_close(m);
    CHECK(!avp_named_mutex_exists("Local\\AvpFsTestNoSuchMutexZ"));
}

AVP_TEST(file_byte_range_locks) {
    AvpArena *a = avp_arena_new(0);
    char *dir = avp_mkdtemp(a, "avpwin_", "", NULL);
    char *path = avp_path_join(a, dir, ".lock");
    /* matchlib.lock_folder: open(".lock", "a+"), seek(0), msvcrt.locking(fd, LK_NBLCK, 1) */
    AvpFile *f1 = avp_fopen(path, "a+", AVP_TM_UTF8), *f2 = avp_fopen(path, "a+", AVP_TM_UTF8);
    REQUIRE(f1 && f2);
    CHECK(avp_filelock_try(avp_fhandle(f1), 0, 1));
    CHECK(!avp_filelock_try(avp_fhandle(f2), 0, 1));
    CHECK_CZ(avp_io_err_class(), "PermissionError");
    CHECK_CZ(avp_io_err_str(a).p, "[Errno 13] Permission denied");
    CHECK(avp_filelock_try(avp_fhandle(f2), 5, 1)); /* a different byte is free */
    CHECK(avp_filelock_unlock(avp_fhandle(f1), 0, 1));
    CHECK(avp_filelock_try(avp_fhandle(f2), 0, 1));
    CHECK(avp_fclose(f1));
    CHECK(avp_fclose(f2)); /* closing releases the locks */
    avp_tempdir_cleanup(dir);
    avp_arena_free(a);
}

AVP_TEST(process_helpers) {
    AvpArena *a = avp_arena_new(0);
    u32 me = avp_getpid();
    CHECK(me != 0 && me == GetCurrentProcessId());
    CHECK(avp_pid_alive(me));
    CHECK(!avp_pid_alive(0x7FFFFFF0u));
    char *img = avp_proc_image_path(a, me);
    CHECK(img != NULL);
    if (img) CHECK(strlen(img) > 4 && ieq(img + strlen(img) - 4, ".exe"));
    u64 ft = 0;
    CHECK(avp_proc_creation_time(me, &ft) && ft > 116444736000000000ULL);
    AvpProcInfoVec pv;
    CHECK(avp_proc_list(a, &pv));
    bool seen = false;
    for (size_t i = 0; i < pv.n; i++)
        if (pv.v[i].pid == me && pv.v[i].exe.n > 0) seen = true;
    CHECK(seen);
    CHECK(avp_getppid() != 0);
    CHECK(avp_set_priority_below_normal());
    CHECK_EQ_U64(GetPriorityClass(GetCurrentProcess()), BELOW_NORMAL_PRIORITY_CLASS);
    CHECK(avp_set_priority_class(NORMAL_PRIORITY_CLASS));
    /* os.cpu_count() = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) on 3.13+ */
    CHECK_EQ_U64(avp_cpu_count(), GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    CHECK(avp_cpu_count() >= 1);
    avp_vec_free(&pv);
    avp_arena_free(a);
}

AVP_TEST(host_and_user) {
    AvpArena *a = avp_arena_new(0);
    /* socket.gethostname() is the computer's DNS host name, which is COMPUTERNAME in lower or upper case */
    char *h = avp_hostname(a), *cn = avp_getenv(a, "COMPUTERNAME");
    CHECK(h && *h);
    if (cn) CHECK(ieq(h, cn));
    /* getpass.getuser(): LOGNAME, USER, LNAME, USERNAME in that order */
    avp_setenv("USERNAME", "somebody");
    avp_unsetenv("LNAME");
    avp_unsetenv("USER");
    avp_unsetenv("LOGNAME");
    CHECK_CZ(avp_getuser(a), "somebody");
    avp_setenv("LOGNAME", "first");
    CHECK_CZ(avp_getuser(a), "first");
    avp_unsetenv("LOGNAME");
    avp_arena_free(a);
}

AVP_TEST(volume_queries) {
    AvpArena *a = avp_arena_new(0);
    char *dir = avp_mkdtemp(a, "avpwin_", "", NULL);
    u32 serial = 0;
    char *fs = NULL;
    CHECK(avp_volume_info(a, dir, &serial, &fs));
    CHECK(serial != 0 && fs && *fs);
    u32 cl = avp_cluster_size(dir);
    CHECK(cl >= 512 && (cl & (cl - 1)) == 0);
    /* a block clone works on ReFS only; a failed clone leaves no destination */
    char *src = avp_path_join(a, dir, "src.bin"), *dst = avp_path_join(a, dir, "dst.bin");
    char block[8192];
    memset(block, 'k', sizeof block);
    CHECK(avp_write_file(src, block, sizeof block));
    bool cloned = avp_file_clone(src, dst, cl);
    if (strcmp(fs, "ReFS") == 0) CHECK(cloned);
    if (!cloned) CHECK(!avp_exists(dst));
    else {
        i64 sz;
        CHECK(avp_getsize(dst, &sz) && sz == (i64)sizeof block);
    }
    avp_tempdir_cleanup(dir);
    avp_arena_free(a);
}

AVP_TEST(console_helpers) {
    /* under CTest stdout is a pipe or a file: not a tty, width falls back to 80 */
    if (!avp_isatty(1)) CHECK_EQ_I64(avp_console_width(), 80);
    CHECK(!avp_isatty(7));
}

AVP_TEST(undecorate_symbols) {
    AvpArena *a = avp_arena_new(0);
    /* ctypes.WinDLL("dbghelp").UnDecorateSymbolName(name, buf, 2048, flags) */
    CHECK_STR(avp_undecorate_symbol(a, "?foo@@YAHH@Z", 0), "int __cdecl foo(int)");
    CHECK_STR(avp_undecorate_symbol(a, "?foo@@YAHH@Z", 0x1000), "foo");
    CHECK_STR(avp_undecorate_symbol(a, "??_7Foo@Bar@@6B@", 0), "const Bar::Foo::`vftable'");
    CHECK_STR(avp_undecorate_symbol(a, ".?AVFoo@Bar@@", 0), ".?AVFoo@Bar@@"); /* not a symbol: the text comes back */
    avp_arena_free(a);
}

static void hex(const char *h, u8 *out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        sscanf(h + 2 * i, "%2x", &v);
        out[i] = (u8)v;
    }
}

AVP_TEST(aes128_decrypt) {
    /* FIPS-197 appendix C.1: key 000102..0f, plaintext 00112233445566778899aabbccddeeff, ciphertext 69c4e0d86a7b0430d8cdb78070b4c55a */
    u8 key[16], ct[16], pt[16], out[32], two[32];
    hex("000102030405060708090a0b0c0d0e0f", key, 16);
    hex("69c4e0d86a7b0430d8cdb78070b4c55a", ct, 16);
    hex("00112233445566778899aabbccddeeff", pt, 16);
    CHECK(avp_aes128_decrypt(key, ct, 16, false, out));
    CHECK_MEM(out, pt, 16);
    /* ECB of two equal blocks; CBC with a zero IV: P1 = D(C1), P2 = D(C2) xor C1 */
    memcpy(two, ct, 16);
    memcpy(two + 16, ct, 16);
    CHECK(avp_aes128_decrypt(key, two, 32, false, out));
    CHECK_MEM(out, pt, 16);
    CHECK_MEM(out + 16, pt, 16);
    CHECK(avp_aes128_decrypt(key, two, 32, true, out));
    CHECK_MEM(out, pt, 16);
    u8 p2[16];
    for (int i = 0; i < 16; i++) p2[i] = (u8)(pt[i] ^ ct[i]);
    CHECK_MEM(out + 16, p2, 16);
}

AVP_TEST_MAIN("win")
