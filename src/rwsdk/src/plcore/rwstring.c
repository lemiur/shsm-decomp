// Reference: tgsm/pbg src/rwsdk/src/plcore/rwstring.c
/*
 * rwstring.c: the core's string function table. _rwStringOpen fills RwGlobals' table with the C
 * library's functions and the core's own replacements for those the library lacks.
 */
#include <stdio.h>
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>
#include <rwsdk/plcore/rwstring.h>

/* Stands in for a NULL string in a stream (tgsm/pbg config/GPLE9G/symbols.txt names it). */
static const RwChar nullString[] = "";

/* Compares two strings, ignoring case; a NULL string compares equal to anything. */
static int StrICmp(const RwChar* string1, const RwChar* string2) {
    if (string1 != NULL && string2 != NULL) {
        RwChar c1;
        RwChar c2;

        do {
            c1 = *string1;
            c2 = *string2;

            if (c1 >= 'A' && c1 <= 'Z') {
                c1 += 'a' - 'A';
            }
            if (c2 >= 'A' && c2 <= 'Z') {
                c2 += 'a' - 'A';
            }

            if (c1 != c2) {
                return c1 - c2;
            }

            string1++;
            string2++;
        } while (c1 != '\0' && c2 != '\0');

        if (c1 != c2) {
            return c1 - c2;
        }
    }

    return 0;
} // 0x802A6DC4

/* Converts a string to upper case in place. */
static RwChar* StrUpr(RwChar* string) {
    if (string != NULL) {
        RwChar c;
        RwChar* cur;

        for (cur = string; (c = *cur) != '\0'; cur++) {
            if (c >= 'a' && c <= 'z') {
                *cur = c - ('a' - 'A');
            }
        }
    }

    return string;
} // 0x802A6E54

/* Converts a string to lower case in place. */
static RwChar* StrLwr(RwChar* string) {
    if (string != NULL) {
        RwChar c;
        RwChar* cur;

        for (cur = string; (c = *cur) != '\0'; cur++) {
            if (c >= 'A' && c <= 'Z') {
                *cur = c + ('a' - 'A');
            }
        }
    }

    return string;
} // 0x802A6E90

/* Finds the first findThis in a string (the terminator included); NULL if there is none. */
static RwChar* StrChr(const RwChar* string, RwInt32 findThis) {
    RwChar c;
    RwChar* found = NULL;
    RwChar findChar = (RwChar)findThis;

    do {
        c = *string;
        if (c == findChar) {
            found = (RwChar*)string;
            break;
        }
        string++;
    } while (c != '\0');

    return found;
} // 0x802A6ECC

/* Finds the last findThis in a string (the terminator included); NULL if there is none. */
static RwChar* StrRChr(const RwChar* string, RwInt32 findThis) {
    RwChar c;
    RwChar* found = NULL;
    RwChar findChar = (RwChar)findThis;

    do {
        c = *string;
        if (c == findChar) {
            found = (RwChar*)string;
        }
        string++;
    } while (c != '\0');

    return found;
} // 0x802A6F00

/* Opens the string module: fills the core's string function table. */
RwBool _rwStringOpen(void) {
    RWSRCGLOBAL(stringFuncs).vecSprintf = sprintf;
    RWSRCGLOBAL(stringFuncs).vecVsprintf = vsprintf;
    RWSRCGLOBAL(stringFuncs).vecStrcpy = strcpy;
    RWSRCGLOBAL(stringFuncs).vecStrncpy = strncpy;
    RWSRCGLOBAL(stringFuncs).vecStrcat = strcat;
    RWSRCGLOBAL(stringFuncs).vecStrncat = strncat;
    RWSRCGLOBAL(stringFuncs).vecStrrchr = StrRChr;
    RWSRCGLOBAL(stringFuncs).vecStrchr = StrChr;
    RWSRCGLOBAL(stringFuncs).vecStrstr = strstr;
    RWSRCGLOBAL(stringFuncs).vecStrcmp = strcmp;
    RWSRCGLOBAL(stringFuncs).vecStrncmp = strncmp;
    RWSRCGLOBAL(stringFuncs).vecStricmp = StrICmp;
    RWSRCGLOBAL(stringFuncs).vecStrlen = strlen;
    RWSRCGLOBAL(stringFuncs).vecStrupr = StrUpr;
    RWSRCGLOBAL(stringFuncs).vecStrlwr = StrLwr;
    RWSRCGLOBAL(stringFuncs).vecStrtok = strtok;
    RWSRCGLOBAL(stringFuncs).vecSscanf = sscanf;

    return TRUE;
} // 0x802A6F30

/* Closes the string module. */
void _rwStringClose(void) {
} // 0x802A7058

/* The size of string's chunk data: the string and its terminator, rounded up to whole words. */
RwInt32 _rwStringStreamGetSize(const RwChar* string) {
    if (string == NULL) {
        string = nullString;
    }

    return (rwstrlen(string) + sizeof(RwChar) + 3) & ~3;
} // 0x802A705C

/* Writes string to stream as a string chunk. */
const RwChar* _rwStringStreamWrite(const RwChar* string, RwStream* stream) {
    RwInt32 size;

    if (string == NULL) {
        string = nullString;
    }

    size = _rwStringStreamGetSize(string);

    if (!RwStreamWriteChunkHeader(stream, rwID_STRING, size)) {
        return NULL;
    }

    if (!RwStreamWrite(stream, string, size)) {
        return NULL;
    }

    return string;
} // 0x802A709C

/* Reads length bytes of a string chunk into nativeString (allocated when NULL). */
static RwChar* StringStreamRead(RwChar* nativeString, RwStream* stream, RwUInt32 length) {
    /* The read buffer is 64-byte aligned (retail aligns the stack frame to 64 bytes for it). */
    RwChar multiByteString[64] __attribute__((aligned(64)));
    RwChar* baseString;

    if (nativeString == NULL) {
        nativeString = (RwChar*)RwMalloc(length, rwMEMHINTDUR_EVENT | rwID_STRING);
        if (nativeString == NULL) {
            RWERROR((E_RW_NOMEM, length));
            return NULL;
        }
    }

    baseString = nativeString;
    while (length != 0) {
        RwUInt32 readSize = length > sizeof(multiByteString) ? sizeof(multiByteString) : length;
        RwUInt32 i;

        if (RwStreamRead(stream, multiByteString, readSize) != readSize) {
            return NULL;
        }

        length -= readSize;
        for (i = 0; i < readSize; i++) {
            baseString[i] = multiByteString[i];
        }
        baseString += readSize;
    }

    return nativeString;
} // 0x802A715C

/*
 * Reads length bytes of a Unicode string chunk into nativeString (allocated when NULL), keeping the
 * low byte of each character.
 */
static RwChar* UnicodeStringStreamRead(RwChar* nativeString, RwStream* stream, RwUInt32 length) {
    /* The read buffer is 64-byte aligned, as in StringStreamRead. */
    RwUInt16 unicodeString[64] __attribute__((aligned(64)));
    RwChar* baseString;
    RwBool allocated = FALSE;

    if (nativeString == NULL) {
        nativeString = (RwChar*)RwMalloc(length, rwMEMHINTDUR_EVENT | rwID_STRING);
        if (nativeString == NULL) {
            RWERROR((E_RW_NOMEM, length));
            return NULL;
        }
        allocated = TRUE;
    }

    baseString = nativeString;
    while (length != 0) {
        RwUInt32 readSize = length > sizeof(unicodeString) ? sizeof(unicodeString) : length;
        RwUInt32 numChars;
        RwUInt32 i;

        if (RwStreamRead(stream, unicodeString, readSize) != readSize) {
            if (allocated) {
                RwFree(nativeString);
            }
            return NULL;
        }

        length -= readSize;
        numChars = readSize / sizeof(RwUInt16);
        for (i = 0; i < numChars; i++) {
            baseString[i] = (RwChar)unicodeString[i];
        }
        baseString += numChars;
    }

    return nativeString;
} // 0x802A72D8

/* Finds the next string chunk in stream and reads it into string (allocated when NULL). */
RwChar* _rwStringStreamFindAndRead(RwChar* string, RwStream* stream) {
    RwUInt32 type;
    RwUInt32 length;
    RwUInt32 version;

    while (_rwStreamReadChunkHeader(stream, &type, &length, &version, NULL)) {
        RwBool versionOk = version >= rwLIBRARYBASEVERSION && version <= rwLIBRARYCURRENTVERSION;

        if (!versionOk) {
            RWERROR((E_RW_BADVERSION));
            return NULL;
        }

        if (type == rwID_STRING) {
            return StringStreamRead(string, stream, length);
        }

        if (type == rwID_UNICODESTRING) {
            return UnicodeStringStreamRead(string, stream, length);
        }

        if (!RwStreamSkip(stream, length)) {
            return NULL;
        }
    }

    return NULL;
} // 0x802A749C
