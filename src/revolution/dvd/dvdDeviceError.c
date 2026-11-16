// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/dvd/dvdDeviceError.c (same SDK build).
// The message texts are kept byte for byte: accented letters are Latin-1 escapes, the Japanese text is written in
// UTF-8 and converted to Shift-JIS by the build (sjiswrap).
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <revolution/sc.h>

/* Drive error codes (the low 24 bits of the immediate buffer after DVDLowRequestError); names invented. */
#define DVD_DEVERR_INVALID_FIELD   0x52100 /* the probe read is out of range: a licensed drive */
#define DVD_DEVERR_INVALID_COMMAND 0x52000
#define DVD_DEVERR_COPY_PROTECTION 0x53100

/* DVDLowReportKey format of the probe (name invented). */
#define DVD_REPORTKEY_RPC_STATE 0x40000

static u8 CheckBuffer[32] ATTRIBUTE_ALIGN(32);

static volatile BOOL lowDone = TRUE; // volatile: set by the DI interrupt callback while __DVDCheckDevice spins on it
static volatile u32 lowIntType = 0;  // volatile: set by the DI interrupt callback while __DVDCheckDevice spins on it

static void __DVDShowDeviceErrorMessage(void);

/*
 * lowCallback
 *
 * Records the interrupt type of the finished probe command.
 */
static void lowCallback(u32 intType) {
    lowIntType = intType;
    lowDone = TRUE;
} // 0x80225610

/*
 * __DVDCheckDevice
 *
 * Probes the drive with commands an unmodified drive rejects in a known way (an out-of-range unencrypted read and a
 * DVD-Video report key). Returns TRUE for a genuine drive or when the check does not apply (development console,
 * old IOS); shows the "unauthorized device" or the fatal message and returns FALSE otherwise.
 */
BOOL __DVDCheckDevice(void) {
    u32 checkOffset = 0x460A0000;
    OSIOSRev iosRev;

    if (OSGetPhysicalMem2Size() == 0x08000000) {
        return TRUE;
    }

    __OSGetIOSRev(&iosRev);
    if (iosRev.major < 30 || iosRev.major >= 254) {
        return TRUE;
    }

    if (__OSDiscLayers == 0x81) {
        checkOffset = 0x7ED40000;
    }

    lowDone = FALSE;
    DVDLowUnencryptedRead(CheckBuffer, sizeof(CheckBuffer), checkOffset, lowCallback);
    while (!lowDone) {
    }

    switch (lowIntType) {
    case DVD_INTTYPE_DE:
        break;
    case DVD_INTTYPE_TC:
        goto invalid;
    default:
        goto fatal;
    }

    lowDone = FALSE;
    DVDLowRequestError(lowCallback);
    while (!lowDone) {
    }

    DVDLowGetImmBufferReg(); /* result unused here (xbret stores both reads in locals nothing reads) */
    switch (lowIntType) {
    case DVD_INTTYPE_TC:
        if ((DVDLowGetImmBufferReg() & 0xFF000000) != 0) {
            goto recover;
        }
        switch (DVDLowGetImmBufferReg() & 0xFFFFFF) {
        case DVD_DEVERR_INVALID_FIELD:
            break;
        default:
            goto invalid;
        }
        break;
    default:
        goto fatal;
    }

    lowDone = FALSE;
    DVDLowReportKey((DVDVideoReportKey*)CheckBuffer, DVD_REPORTKEY_RPC_STATE, 0, lowCallback);
    while (!lowDone) {
    }

    switch (lowIntType) {
    case DVD_INTTYPE_DE:
        break;
    case DVD_INTTYPE_TC:
        goto invalid;
    default:
        goto fatal;
    }

    lowDone = FALSE;
    DVDLowRequestError(lowCallback);
    while (!lowDone) {
    }

    DVDLowGetImmBufferReg();
    switch (lowIntType) {
    case DVD_INTTYPE_TC:
        if ((DVDLowGetImmBufferReg() & 0xFF000000) != 0) {
            goto recover;
        }
        switch (DVDLowGetImmBufferReg() & 0xFFFFFF) {
        case DVD_DEVERR_COPY_PROTECTION:
        case DVD_DEVERR_INVALID_COMMAND:
            break;
        default:
            goto invalid;
        }
        break;
    default:
        goto fatal;
    }

    return TRUE;

invalid:
    __DVDShowDeviceErrorMessage();
    return FALSE;

recover:
    return FALSE;

fatal:
    __DVDShowFatalMessage();
    return FALSE;
} // 0x80225620

/* The "unauthorized device" message per language (indexed by SCGetLanguage). */
const char* const __DVDDeviceErrorMessage[] = {
    "\n\n\nエラーコード００１。\n"
    "不明なデバイスが見つかりました。",

    "\n\n\nError #001,\n"
    "unauthorized device has been detected.",

    "\n\n\nFehler #001:\n"
    "Es wurde eine unzul\xE4ssige Komponente\n"
    "entdeckt.",

    "\n\n\nErreur 001:\n"
    "un dispositif non autoris\xE9 a \xE9t\xE9 d\xE9tect\xE9.",

    "\n\n\nError 001:\n"
    "Se ha detectado un dispositivo no\n"
    "autorizado.",

    "\n\n\nErrore #001:\n"
    "rilevato un dispositivo non autorizzato.",

    "\n\n\nFout #001:\n"
    "ongeoorloofd onderdeel gevonden."
};

/*
 * __DVDShowDeviceErrorMessage
 *
 * Shows the "unauthorized device has been detected" screen in the console's language and halts.
 */
static void __DVDShowDeviceErrorMessage(void) {
    const char* message;
    const char* const* messageList;
    GXColor bg = {0, 0, 0, 0};
    GXColor fg = {255, 255, 255, 0};

    if (SCGetLanguage() == SC_LANG_JAPANESE) {
        OSSetFontEncode(OS_FONT_ENCODE_SJIS);
    } else {
        OSSetFontEncode(OS_FONT_ENCODE_ANSI);
    }

    messageList = __DVDDeviceErrorMessage;

    if (SCGetLanguage() > SC_LANG_DUTCH) {
        message = messageList[SC_LANG_ENGLISH];
    } else {
        message = messageList[SCGetLanguage()];
    }

    OSFatal(fg, bg, message);
} // (inlined)
