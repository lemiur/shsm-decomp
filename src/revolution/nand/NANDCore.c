// Reference: zeldaret/tp libs/revolution/src/nand/NANDCore.c
//
// Retail is the release build without assertions. Functions of the reference that retail does not carry:
// NANDGetCurrentDir and NANDGetHomeDir (nothing in the game calls them, so the linker dead-strips them), the
// debug-only nandReportErrorCode body and the nandLoggingCallback of later SDKs (retail logs without a
// callback), the error-message calls and NANDSetAutoErrorMessaging.
#include <revolution/esp.h>
#include <revolution/fs.h>
#include <revolution/nand.h>
#include <revolution/nand/NANDCore.h>
#include <revolution/os.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define BUILD_DATE "Feb 27 2009"
#define BUILD_TIME "10:05:16"

/* Upper bound of the ISFS error range (name invented; the ISFS codes are -101 and below). */
#define NAND_ISFS_ERROR_BASE (-100)

const char* __NANDVersion = "<< RVL_SDK - NAND \trelease build: " BUILD_DATE " " BUILD_TIME " (0x4302_145) >>";

/* Shutdown event on which ISFS is shut down: the power-off event (SD_EVENT_POWER_OFF in OSReset.c, which keeps
   it private; name invented). */
#define NAND_SHUTDOWN_EVENT 2

enum LibState {
    STATE_NOT_INITIALIZED,
    STATE_WORKING,
    STATE_INITIALIZED
};

static enum LibState s_libState = STATE_NOT_INITIALIZED;
static char s_currentDir[64] __attribute__((aligned(32))) = "/";
static char s_homeDir[64] __attribute__((aligned(32))) = "";

static BOOL nandOnShutdown(BOOL final, u32 event);
static void nandShutdownCallback(ISFSError result, void* ctxt);

static OSShutdownFunctionInfo s_shutdownFuncInfo = {
    nandOnShutdown,
    255
};

static void nandGetTypeCallback(ISFSError result, void* ctxt);

/*
 * nandRemoveTailToken
 *
 * Drops the last component of an absolute path.
 */
void nandRemoveTailToken(char* newpath, const char* oldpath) {
    if (oldpath[0] == '/' && oldpath[1] == '\0') {
        newpath[0] = '/';
        newpath[1] = '\0';
    } else {
        int i = 0;

        for (i = (int)(strlen(oldpath)) - 1; i >= 0; --i) {
            if (oldpath[i] == '/') {
                if (i != 0) {
                    strncpy(newpath, oldpath, (u32)i);
                    newpath[i] = '\0';
                    break;
                } else {
                    newpath[0] = '/';
                    newpath[1] = '\0';
                    break;
                }
            }
        }
    }
} // 0x80245B50

/*
 * nandGetHeadToken
 *
 * Splits a relative path at its first '/': the leading component into token, the rest into newpath.
 */
void nandGetHeadToken(char* token, char* newpath, const char* oldpath) {
    unsigned int i = 0;

    for (i = 0; i <= strlen(oldpath); ++i) {
        if (oldpath[i] == '/') {
            strncpy(token, oldpath, i);
            token[i] = '\0';

            if (oldpath[i + 1] == '\0') {
                newpath[0] = '\0';
            } else {
                strcpy(newpath, oldpath + i + 1);
            }

            break;
        } else if (oldpath[i] == '\0') {
            strncpy(token, oldpath, i);
            token[i] = '\0';
            newpath[0] = '\0';
            break;
        }
    }
} // 0x80245C30

/*
 * nandGetRelativeName
 *
 * The last component of an absolute path ("" for the root).
 */
void nandGetRelativeName(char* name, const char* path) {
    if (strcmp("/", path) == 0) {
        strcpy(name, "");
    } else {
        int i = 0;

        for (i = (int)(strlen(path) - 1); i >= 0; --i) {
            if (path[i] == '/') {
                break;
            }
        }

        strcpy(name, path + i + 1);
    }
} // 0x80245D10

/*
 * nandConvertPath
 *
 * Resolves a relative path against a working directory, one component at a time ("." and ".." included).
 */
void nandConvertPath(char* abspath, const char* wd, const char* relpath) {
    char token[128];
    char new_relpath[128];

    if (strlen(relpath) == 0) {
        strcpy(abspath, wd);
        return;
    }

    nandGetHeadToken(token, new_relpath, relpath);

    if (strcmp(token, ".") == 0) {
        nandConvertPath(abspath, wd, new_relpath);
    } else if (strcmp(token, "..") == 0) {
        char new_wd[128];
        nandRemoveTailToken(new_wd, wd);
        nandConvertPath(abspath, new_wd, new_relpath);
    } else if (token[0] != '\0') {
        char new_wd[128];
        if (strcmp(wd, "/") == 0) {
            sprintf(new_wd, "/%s", token);
        } else {
            sprintf(new_wd, "%s/%s", wd, token);
        }

        nandConvertPath(abspath, new_wd, new_relpath);
    } else {
        strcpy(abspath, wd);
    }
} // 0x80245DB0

/*
 * nandIsRelativePath
 */
static BOOL nandIsRelativePath(const char* path) {
    if (path[0] == '/') {
        return FALSE;
    } else {
        return TRUE;
    }
}

/*
 * nandIsPrivatePath
 */
BOOL nandIsPrivatePath(const char* path) {
    if (strncmp(path, "/shared2", 8) == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
} // 0x80245EF0

/*
 * nandIsUnderPrivatePath
 */
BOOL nandIsUnderPrivatePath(const char* path) {
    if (strncmp(path, "/shared2/", 9) == 0 && path[9] != '\0') {
        return TRUE;
    } else {
        return FALSE;
    }
} // 0x80245F30

/*
 * nandIsInitialized
 */
BOOL nandIsInitialized(void) {
    if (s_libState == STATE_INITIALIZED) {
        return TRUE;
    } else {
        return FALSE;
    }
} // 0x80245F90

/*
 * nandReportErrorCode
 *
 * Debug builds report an error code here. The release build keeps only the range test: -23 is the lowest IOS
 * code and ISFS codes start below -100, so the codes in between are neither (the test is evaluated and unused).
 */
static void nandReportErrorCode(const ISFSError err) {
    (void)(err < IOS_ERROR_INVALID_SIZE && err > NAND_ISFS_ERROR_BASE);
}

/*
 * nandConvertErrorCode
 *
 * Maps an ISFS or IOS error to a NAND result. The serious ones are also written to the NAND log.
 */
s32 nandConvertErrorCode(const ISFSError err) {
    const int ERRMAP[] = {
        ISFS_ERROR_OK,             NAND_RESULT_OK,
        ISFS_ERROR_ACCESS,         NAND_RESULT_ACCESS,
        ISFS_ERROR_CORRUPT,        NAND_RESULT_CORRUPT,
        ISFS_ERROR_ECC_CRIT,       NAND_RESULT_ECC_CRIT,
        ISFS_ERROR_EXISTS,         NAND_RESULT_EXISTS,
        ISFS_ERROR_HMAC,           NAND_RESULT_AUTHENTICATION,
        ISFS_ERROR_INVALID,        NAND_RESULT_INVALID,
        ISFS_ERROR_MAXBLOCKS,      NAND_RESULT_MAXBLOCKS,
        ISFS_ERROR_MAXFD,          NAND_RESULT_MAXFD,
        ISFS_ERROR_MAXFILES,       NAND_RESULT_MAXFILES,
        ISFS_ERROR_MAXDEPTH,       NAND_RESULT_MAXDEPTH,
        ISFS_ERROR_NOEXISTS,       NAND_RESULT_NOEXISTS,
        ISFS_ERROR_NOTEMPTY,       NAND_RESULT_NOTEMPTY,
        ISFS_ERROR_NOTREADY,       NAND_RESULT_UNKNOWN,
        ISFS_ERROR_OPENFD,         NAND_RESULT_OPENFD,
        ISFS_ERROR_UNKNOWN,        NAND_RESULT_UNKNOWN,
        ISFS_ERROR_BUSY,           NAND_RESULT_BUSY,
        ISFS_ERROR_SHUTDOWN,       NAND_RESULT_FATAL_ERROR,

        IOS_ERROR_ACCESS,          NAND_RESULT_ACCESS,
        IOS_ERROR_EXISTS,          NAND_RESULT_EXISTS,
        IOS_ERROR_INTR,            NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID,         NAND_RESULT_INVALID,
        IOS_ERROR_MAX,             NAND_RESULT_UNKNOWN,
        IOS_ERROR_NOEXISTS,        NAND_RESULT_NOEXISTS,
        IOS_ERROR_QEMPTY,          NAND_RESULT_UNKNOWN,
        IOS_ERROR_QFULL,           NAND_RESULT_BUSY,
        IOS_ERROR_UNKNOWN,         NAND_RESULT_UNKNOWN,
        IOS_ERROR_NOTREADY,        NAND_RESULT_UNKNOWN,
        IOS_ERROR_ECC,             NAND_RESULT_UNKNOWN,
        IOS_ERROR_ECC_CRIT,        NAND_RESULT_ECC_CRIT,
        IOS_ERROR_BADBLOCK,        NAND_RESULT_UNKNOWN,

        IOS_ERROR_INVALID_OBJTYPE, NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID_RNG,     NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID_FLAG,    NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID_FORMAT,  NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID_VERSION, NAND_RESULT_UNKNOWN,
        IOS_ERROR_INVALID_SIGNER,  NAND_RESULT_UNKNOWN,
        IOS_ERROR_FAIL_CHECKVALUE, NAND_RESULT_UNKNOWN,
        IOS_ERROR_FAIL_INTERNAL,   NAND_RESULT_UNKNOWN,
        IOS_ERROR_FAIL_ALLOC,      NAND_RESULT_ALLOC_FAILED,
        IOS_ERROR_INVALID_SIZE,    NAND_RESULT_UNKNOWN,
    };

    int i = 0;

    if (err >= 0) {
        return err;
    }

    for (; i < sizeof(ERRMAP) / 4; i = i + 2) {
        if (ERRMAP[i] == err) {
            if (err == ISFS_ERROR_ECC_CRIT || err == ISFS_ERROR_HMAC || err == ISFS_ERROR_UNKNOWN || err == IOS_ERROR_UNKNOWN || err == IOS_ERROR_ECC_CRIT) {
                char buf[128] __attribute__((aligned(64)));
                sprintf(buf, "ISFS error code: %d", err);
                NANDLoggingAddMessageAsync(NULL, buf);
            }

            nandReportErrorCode(err);
            return ERRMAP[i + 1];
        }
    }

    OSReport("CAUTION!  Unexpected error code [%d] was found.\n", err);
    {
        char buf[128] __attribute__((aligned(64)));
        sprintf(buf, "ISFS unexpected error code: %d", err);
        NANDLoggingAddMessageAsync(NULL, buf);
    }

    nandReportErrorCode(err);
    return NAND_RESULT_UNKNOWN;
} // 0x80245FB0

/*
 * nandGenerateAbsPath
 *
 * Absolute form of a library path: relative paths are resolved against the current directory, a trailing '/'
 * is cut.
 */
void nandGenerateAbsPath(char* absPath, const char* path) {
    if (strlen(path) == 0) {
        strcpy(absPath, "");
    } else if (nandIsRelativePath(path)) {
        nandConvertPath(absPath, s_currentDir, path);
    } else {
        u32 len;
        strcpy(absPath, path);

        len = strlen(absPath);
        if (len > 0) {
            if ((absPath[len - 1] == '/') && (len - 1 != 0)) {
                absPath[len - 1] = '\0';
            }
        }
    }
} // 0x80246120

/*
 * nandGetParentDirectory
 */
void nandGetParentDirectory(char* parentDir, const char* absPath) {
    int i = 0;
    for (i = (int)strlen(absPath); i >= 0; --i) {
        if (absPath[i] == '/') {
            break;
        }
    }

    if (i == 0) {
        strcpy(parentDir, "/");
    } else {
        strncpy(parentDir, absPath, (u32)i);
        parentDir[i] = '\0';
    }
} // 0x802461F0

/*
 * NANDInit
 *
 * Opens ISFS and ES, looks up the title's data directory (home and current directory) and registers the
 * shutdown handler.
 */
s32 NANDInit(void) {
    BOOL enabled = OSDisableInterrupts();

    if (s_libState == STATE_WORKING) {
        OSRestoreInterrupts(enabled);
        return NAND_RESULT_BUSY;
    } else if (s_libState == STATE_INITIALIZED) {
        OSRestoreInterrupts(enabled);
        return NAND_RESULT_OK;
    } else {
        ISFSError result = ISFS_ERROR_UNKNOWN;
        s_libState = STATE_WORKING;
        OSRestoreInterrupts(enabled);

        result = ISFS_OpenLib();
        if (result == 0) {
            s32 rv;
            ESTitleId id;

            rv = ESP_InitLib();

            if (rv == 0) {
                rv = ESP_GetTitleId(&id);
            }

            if (rv == 0) {
                rv = ESP_GetDataDir(id, s_homeDir);
            }

            if (rv == 0) {
                strcpy(s_currentDir, s_homeDir);
            }

            ESP_CloseLib();

            if (rv != 0) {
                OSReport("Failed to set home directory.\n");
            }

            OSRegisterShutdownFunction(&s_shutdownFuncInfo);
            enabled = OSDisableInterrupts();
            s_libState = STATE_INITIALIZED;
            OSRestoreInterrupts(enabled);
            OSRegisterVersion(__NANDVersion);
            return NAND_RESULT_OK;
        } else {
            enabled = OSDisableInterrupts();
            s_libState = STATE_NOT_INITIALIZED;
            OSRestoreInterrupts(enabled);
            return nandConvertErrorCode(result);
        }
    }
} // 0x802462A0

/*
 * nandOnShutdown
 *
 * On power-off, asks ISFS to shut down and waits for it for up to half a second.
 */
static BOOL nandOnShutdown(BOOL final, u32 event) {
    if (!final) {
        if (event == NAND_SHUTDOWN_EVENT) {
            BOOL flag = FALSE;
            OSTime t = OSGetTime();
            ISFS_ShutdownAsync(nandShutdownCallback, (void*)&flag);

            while (OSTicksToMilliseconds(OSGetTime() - t) < 500) {
                if (flag) {
                    break;
                }
            }
        }

        return TRUE;
    } else {
        return TRUE;
    }
} // 0x802463D0

/*
 * nandShutdownCallback
 */
static void nandShutdownCallback(ISFSError result, void* ctxt) {
    (void)result;
    *(BOOL*)ctxt = TRUE;
} // 0x802464A0

/*
 * nandCallback
 *
 * Completion callback of the async ISFS calls: converts the result and hands it to the caller's callback.
 */
void nandCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;
    ((NANDCallback)(b->callback))(nandConvertErrorCode(result), b);
} // 0x802464B0

/*
 * nandGetType
 */
static ISFSError nandGetType(const char* path, u8* type, NANDCommandBlock* block, const BOOL async_flag, const BOOL privilege_flag) {
    if (strlen(path) == 0) {
        return ISFS_ERROR_INVALID;
    }

    if (async_flag) {
        nandGenerateAbsPath(block->absPath, path);
        if (!privilege_flag && nandIsUnderPrivatePath(block->absPath)) {
            return ISFS_ERROR_ACCESS;
        } else {
            block->type = type;
            return ISFS_ReadDirAsync((u8*)(block->absPath), NULL, &(block->num), nandGetTypeCallback, block);
        }
    } else {
        char absPath[64] = "";
        nandGenerateAbsPath(absPath, path);

        if (!privilege_flag && nandIsUnderPrivatePath(absPath)) {
            return ISFS_ERROR_ACCESS;
        } else {
            u32 dmy = 0;
            ISFSError err = ISFS_ReadDir((u8*)absPath, NULL, &dmy);

            if (err == ISFS_ERROR_OK || err == ISFS_ERROR_ACCESS) {
                *type = NAND_FILE_TYPE_DIR;
                err = ISFS_ERROR_OK;
            } else if (err == ISFS_ERROR_INVALID) {
                *type = NAND_FILE_TYPE_FILE;
                err = ISFS_ERROR_OK;
            }

            return err;
        }
    }
} // 0x802464F0

/*
 * NANDPrivateGetTypeAsync
 */
s32 NANDPrivateGetTypeAsync(const char* path, u8* type, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    return nandConvertErrorCode(nandGetType(path, type, block, TRUE, TRUE));
} // 0x802467D0

/*
 * nandGetTypeCallback
 */
static void nandGetTypeCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;

    if (result == ISFS_ERROR_OK || result == ISFS_ERROR_ACCESS) {
        *(b->type) = NAND_FILE_TYPE_DIR;
        result = ISFS_ERROR_OK;
    } else if (result == ISFS_ERROR_INVALID) {
        *(b->type) = NAND_FILE_TYPE_FILE;
        result = ISFS_ERROR_OK;
    }

    ((NANDCallback)(b->callback))(nandConvertErrorCode(result), b);
} // 0x80246820

/*
 * nandGetHomeDir
 */
const char* nandGetHomeDir(void) {
    return s_homeDir;
} // 0x802468A0

/*
 * NANDInitBanner
 *
 * Fills a channel banner: signature, flags and the two (title, comment) lines. An empty line becomes a space.
 */
void NANDInitBanner(NANDBanner* bnr, u32 const flag, const u16* title, const u16* comment) {
    memset(bnr, 0, sizeof(NANDBanner));
    bnr->signature = NAND_BANNER_SIGNATURE;
    bnr->flag = flag;

    if (wcscmp((wchar_t*)title, L"") == 0) {
        wcsncpy((wchar_t*)bnr->comment[0], L" ", NAND_BANNER_TITLE_MAX);
    } else {
        wcsncpy((wchar_t*)bnr->comment[0], (wchar_t*)title, NAND_BANNER_TITLE_MAX);
    }

    if (wcscmp((wchar_t*)comment, L"") == 0) {
        wcsncpy((wchar_t*)bnr->comment[1], L" ", NAND_BANNER_TITLE_MAX);
    } else {
        wcsncpy((wchar_t*)bnr->comment[1], (wchar_t*)comment, NAND_BANNER_TITLE_MAX);
    }
} // 0x802468B0
