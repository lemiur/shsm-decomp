/* The Win32 calls the Python tools made through ctypes, winreg, msvcrt, socket and getpass: registry reads, named
 * mutexes, byte-range file locks, process helpers, volume queries, console helpers, process priority, host/user
 * names, error-message formatting. Failures are recorded the way fs.h records them (avp_io_err(): an OSError with
 * its winerror), so a caller prints what Python would have printed for the exception.
 *
 * What is NOT here: the window/input/GDI calls of the MCP's input and capture modules (SendInput, EnumWindows,
 * PrintWindow, ...) and process-memory access (ReadProcessMemory, VirtualQueryEx): those modules call <windows.h>
 * directly; they use avp_win_error_message()/avp_io_fail_win() for the text of a failure. The Job object
 * (matchlib.kill_children_with_parent) belongs to proc.h.
 */
#pragma once
#include "fs.h"

/* ---- Win32 errors ------------------------------------------------------------------------------------------------ */
/* FormatMessageW(system default language) with the trailing dots and whitespace removed, as OSError's text:
 * 2 -> "The system cannot find the file specified". Unknown codes give "Unknown error <n>" as Python does. */
AvpStr avp_win_error_message(AvpArena *a, unsigned code);
/* CPython's winerror -> errno table (PC/errmap.h): 2 -> ENOENT, 5 -> EACCES, 183 -> EEXIST, 145 -> ENOTEMPTY, ... */
int avp_winerror_to_errno(unsigned winerror);
/* "[WinError 5] Access is denied" -- the text of OSError(None, msg, None, winerror) without filename. */
AvpStr avp_win_error_str(AvpArena *a, unsigned winerror);

/* ---- registry (winreg) ------------------------------------------------------------------------------------------- */
typedef enum {
    AVP_HKEY_CLASSES_ROOT,
    AVP_HKEY_CURRENT_USER,
    AVP_HKEY_LOCAL_MACHINE,
    AVP_HKEY_USERS,
    AVP_HKEY_CURRENT_CONFIG
} AvpRegRoot;
enum { AVP_KEY_WOW64_DEFAULT = 0, AVP_KEY_WOW64_64KEY = 0x0100, AVP_KEY_WOW64_32KEY = 0x0200 };
enum { AVP_REG_NONE = 0, AVP_REG_SZ = 1, AVP_REG_EXPAND_SZ = 2, AVP_REG_BINARY = 3, AVP_REG_DWORD = 4,
       AVP_REG_MULTI_SZ = 7, AVP_REG_QWORD = 11 };
typedef struct {
    u32 type;         /* AVP_REG_* */
    AvpStr str;       /* REG_SZ, REG_EXPAND_SZ (not expanded): the text up to the first NUL, as winreg returns it */
    u64 num;          /* REG_DWORD, REG_QWORD */
    AvpStr bin;       /* REG_BINARY (and any other type): the raw bytes */
    AvpStrVec multi;  /* REG_MULTI_SZ: the strings */
} AvpRegValue;
/* with winreg.OpenKey(root, subkey, 0, KEY_READ | wow64) as key: value, type = winreg.QueryValueEx(key, name)
 * name "" or NULL is the key's default value. false: FileNotFoundError ([WinError 2]) and the other OSErrors. */
bool avp_reg_get(AvpArena *a, AvpRegRoot root, const char *subkey, unsigned wow64, const char *name, AvpRegValue *out);
/* The same for a string value: false (without an error recorded) when the value is not REG_SZ/REG_EXPAND_SZ. */
bool avp_reg_get_string(AvpArena *a, AvpRegRoot root, const char *subkey, unsigned wow64, const char *name,
                        AvpStr *out);

/* ---- named mutexes (System.Threading.Mutex / gamelock.ps1) ------------------------------------------------------- */
typedef struct AvpNamedMutex AvpNamedMutex;
/* Creates or opens the named mutex "Global\<name>", falling back to "Local\<name>" when Global is refused; not owned
 * yet. NULL on failure. */
AvpNamedMutex *avp_named_mutex_open(const char *name);
enum { AVP_WAIT_ACQUIRED = 0, AVP_WAIT_ABANDONED = 1 /* acquired: the holder died */, AVP_WAIT_TIMEOUT = 2, AVP_WAIT_FAILED = 3 };
/* WaitOne(timeout_ms) (0xFFFFFFFF: forever). A mutex is owned by the thread that waited: release it on that thread. */
int avp_named_mutex_wait(AvpNamedMutex *m, u32 timeout_ms);
bool avp_named_mutex_release(AvpNamedMutex *m);
void avp_named_mutex_close(AvpNamedMutex *m);                    /* releases ownership held by the closing thread's handle */
bool avp_named_mutex_exists(const char *full_name);          /* OpenMutexW(SYNCHRONIZE, name): true when it exists (any session) */

/* ---- byte-range file locks (msvcrt.locking, LockFileEx) ---------------------------------------------------------- */
/* Exclusive, non-blocking lock of `len` bytes at `offset` of an open file (avp_fhandle). msvcrt.locking(fd,
 * LK_NBLCK, 1) after seek(0) is avp_filelock_try(h, 0, 1). false: OSError (EACCES "Permission denied") when held. */
bool avp_filelock_try(void *handle, i64 offset, i64 len);
bool avp_filelock_unlock(void *handle, i64 offset, i64 len);   /* msvcrt.locking(fd, LK_UNLCK, n) */

/* ---- processes --------------------------------------------------------------------------------------------------- */
/* os.getpid() is avp_getpid in proc.h. */
u32 avp_getppid(void);                                 /* os.getppid() */
/* lane_lock.pid_alive: OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION); access denied counts as alive; alive when
 * GetExitCodeProcess says STILL_ACTIVE. */
bool avp_pid_alive(u32 pid);
char *avp_proc_image_path(AvpArena *a, u32 pid);       /* QueryFullProcessImageNameW, NULL when it cannot be read */
bool avp_proc_creation_time(u32 pid, u64 *filetime);   /* GetProcessTimes: creation FILETIME (100 ns since 1601) */
typedef struct {
    u32 pid, parent_pid, threads;
    AvpStr exe;    /* PROCESSENTRY32W.szExeFile in UTF-8 */
} AvpProcInfo;
typedef AVP_VEC(AvpProcInfo) AvpProcInfoVec;
bool avp_proc_list(AvpArena *a, AvpProcInfoVec *out);  /* Toolhelp snapshot in snapshot order */
bool avp_set_priority_below_normal(void);              /* priority.lower(): BELOW_NORMAL_PRIORITY_CLASS for this process */
bool avp_set_priority_class(u32 priority_class);       /* SetPriorityClass(GetCurrentProcess(), c) */
/* os.cpu_count() is avp_cpu_count in pool.h. */

/* ---- machine and user -------------------------------------------------------------------------------------------- */
char *avp_hostname(AvpArena *a);                       /* socket.gethostname() */
char *avp_getuser(AvpArena *a);                        /* getpass.getuser(): LOGNAME, USER, LNAME, USERNAME, else the account name */

/* ---- volumes (objcache) ------------------------------------------------------------------------------------------ */
/* The volume holding `path`: serial number and file system name ("NTFS", "ReFS"); false when unknown. */
bool avp_volume_info(AvpArena *a, const char *path, u32 *serial, char **fs_name);
/* Bytes per cluster of the volume holding `path` (sectors per cluster * bytes per sector); 0 when unknown. */
u32 avp_cluster_size(const char *path);
/* A ReFS block clone of src into the new file dst (FSCTL_DUPLICATE_EXTENTS_TO_FILE): dst shares src's clusters until
 * either is written. true when done; otherwise dst does not exist. cluster from avp_cluster_size. */
bool avp_file_clone(const char *src, const char *dst, u32 cluster);

/* ---- console ----------------------------------------------------------------------------------------------------- */
bool avp_isatty(int fd);                               /* 0 stdin, 1 stdout, 2 stderr: a character device (console) */
int avp_console_width(void);                           /* the console's columns, or 80 (shutil.get_terminal_size fallback) */
bool avp_kbhit(void);                                  /* msvcrt.kbhit() */
u32 avp_getwch(void);                                  /* msvcrt.getwch(): one UTF-16 unit, no echo */

/* ---- symbols and crypto ------------------------------------------------------------------------------------------ */
/* dbghelp's UnDecorateSymbolName(name, flags): the undecorated text (names are Latin-1 bytes in Python's ctypes use,
 * here UTF-8/ASCII), or an empty string when it fails. UNDNAME_NAME_ONLY = 0x1000. Serialized: dbghelp is not
 * thread-safe. */
AvpStr avp_undecorate_symbol(AvpArena *a, const char *name, u32 flags);
/* AES-128 ECB (cbc false) or CBC with a zero IV through BCrypt, decrypting n bytes (a multiple of 16) into out
 * (proto_extract.aes_decrypt). false on any BCrypt failure. */
bool avp_aes128_decrypt(const u8 key[16], const u8 *data, size_t n, bool cbc, u8 *out);
