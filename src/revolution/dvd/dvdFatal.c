// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/dvd/dvdFatal.c (same SDK build).
// The message texts are kept byte for byte: accented letters are Latin-1 escapes, the Japanese text is written in
// UTF-8 and converted to Shift-JIS by the build (sjiswrap).
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <revolution/dvd/__dvd.h>
#include <revolution/sc.h>

static void (*FatalFunc)(void) = NULL;

/* The fatal-error message per language (indexed by SCGetLanguage), for each group of game regions. */
const char* const __DVDErrorMessageDefault[] = {
    // SC_LANG_JAPANESE
    "\n\n\nエラーが発生しました。\n\n"
    "イジェクトボタンを押してディスクを取り出してか\n"
    "ら、本体の電源をOFFにして、本体の取扱説明書の\n"
    "指示に従ってください。",

    // SC_LANG_ENGLISH
    "\n\n\nAn error has occurred.\n"
    "Press the Eject Button, remove the\n"
    "Game Disc, and turn off the power to the\n"
    "console. Please read the Wii Operations\n"
    "Manual for further instructions.",

    // SC_LANG_GERMAN
    "\n\n\nEin Fehler ist aufgetreten.\n"
    "Dr""\xFC""cke den Ausgabeknopf, entnimm die\n"
    "Disc und schalte die Wii-Konsole aus.\n"
    "Bitte lies die Wii-Bedienungsanleitung,\n"
    "um weitere Informationen zu erhalten.",

    // SC_LANG_FRENCH
    "\n\n\nUne erreur est survenue.\n"
    "Appuyez sur le bouton EJECT, retirez\n"
    "le disque et \xE9teignez la console.\n"
    "Veuillez vous r""\xE9""f\xE9rer au Mode d'emploi\n"
    "de la Wii pour plus de d\xE9tails.",

    // SC_LANG_SPANISH
    "\n\n\nOcurri\xF3 un Error.\n"
    "Oprime el Bot\xF3n EJECT, saca el disco\n"
    "y apaga la consola. Consulta el manual\n"
    "de operaciones de la consola Wii para\n"
    "obtener m\xE1s informaci\xF3n.",

    // SC_LANG_ITALIAN
    "\n\n\nSi \xE8 verificato un errore.\n"
    "Premi il pulsante EJECT, estrai il disco\n"
    "e spegni la console. Per maggiori\n"
    "informazioni, consulta il manuale di\n"
    "istruzioni della console Wii.",

    // SC_LANG_DUTCH
    "\n\n\nEr is een fout opgetreden.\n"
    "Druk op de EJECT-knop, verwijder de\n"
    "disk en zet het Wii-systeem uit. Lees\n"
    "de Wii-handleiding voor meer informatie."
};

const char* const __DVDErrorMessageEurope[] = {
    // SC_LANG_JAPANESE
    "\n\n\nエラーが発生しました。\n\n"
    "イジェクトボタンを押してディスクを取り出してか\n"
    "ら、本体の電源をOFFにして、本体の取扱説明書の\n"
    "指示に従ってください。",

    // SC_LANG_ENGLISH
    "\n\n\nAn error has occurred.\n"
    "Press the EJECT Button, remove the Disc,\n"
    "and turn off the power to the console.\n"
    "Please refer to the Wii Operations Manual\n"
    "for details.",

    // SC_LANG_GERMAN
    "\n\n\nEin Fehler ist aufgetreten.\n"
    "Dr""\xFC""cke den Ausgabeknopf, entnimm die\n"
    "Disc und schalte die Wii-Konsole aus.\n"
    "Bitte lies die Wii-Bedienungsanleitung,\n"
    "um weitere Informationen zu erhalten.",

    // SC_LANG_FRENCH
    "\n\n\nUne erreur est survenue.\n"
    "Appuyez sur le bouton EJECT, retirez\n"
    "le disque et \xE9teignez la console.\n"
    "Veuillez vous r""\xE9""f\xE9rer au mode d'emploi\n"
    "Wii pour plus de d\xE9tails.",

    // SC_LANG_SPANISH
    "\n\n\nSe ha producido un error.\n"
    "Pulsa el Bot\xF3n EJECT, extrae el disco y\n"
    "apaga la consola. Consulta el manual de\n"
    "instrucciones de la consola Wii para\n"
    "obtener m\xE1s informaci\xF3n.",

    // SC_LANG_ITALIAN
    "\n\n\nSi \xE8 verificato un errore.\n"
    "Premi il pulsante EJECT, estrai il disco\n"
    "e spegni la console. Per maggiori\n"
    "informazioni, consulta il manuale di\n"
    "istruzioni della console Wii.",

    // SC_LANG_DUTCH
    "\n\n\nEr is een fout opgetreden.\n"
    "Druk op de EJECT-knop, verwijder de\n"
    "disk en zet het Wii-systeem uit. Lees\n"
    "de Wii-handleiding voor meer informatie."

};

/* Chinese and Korean games. The table has two entries only: a language from SC_LANG_GERMAN to SC_LANG_DUTCH
 * indexes past its end (an SDK quirk, kept). */
const char* __DVDErrorMessageChinaKorea[] = {
    // SC_LANG_JAPANESE
    "\n\nエラーコード１０４。\n"
    "エラーが発生しました。\n\n"
    "イジェクトボタンを押してディスクを取り出してか\n"
    "ら、本体の電源をOFFにして、本体の取扱説明書の\n"
    "指示に従ってください。",

    // SC_LANG_ENGLISH (and every language after SC_LANG_DUTCH)
    "\n\nError #104,\n"
    "An error has occurred.\n"
    "Press the EJECT Button, remove the\n"
    "Game Disc, and turn the power off.\n"
    "Please read the Wii Operations Manual\n"
    "for more information."
};

/*
 * __DVDShowFatalMessage
 *
 * Shows the "eject the disc and turn off the console" screen in the console's language and halts.
 */
void __DVDShowFatalMessage(void) {
    const char* msg;
    const char* const* msgArray;
    const GXColor bgColor = {0, 0, 0, 0};
    const GXColor textColor = {255, 255, 255, 0};

    if (SCGetLanguage() == SC_LANG_JAPANESE) {
        OSSetFontEncode(OS_FONT_ENCODE_SJIS);
    } else {
        OSSetFontEncode(OS_FONT_ENCODE_ANSI);
    }

    switch (SCGetProductGameRegion()) {
    default:
        msgArray = __DVDErrorMessageDefault;
        break;
    case SC_PRODUCT_GAME_REGION_EU:
        msgArray = __DVDErrorMessageEurope;
        break;
    case SC_PRODUCT_GAME_REGION_KR:
    case SC_PRODUCT_GAME_REGION_CN:
        msgArray = __DVDErrorMessageChinaKorea;
        break;
    }

    msg = (SCGetLanguage() > SC_LANG_DUTCH) ? msgArray[SC_LANG_ENGLISH] : msgArray[SCGetLanguage()];

    OSFatal(textColor, bgColor, msg);
} // 0x802254A0

/*
 * DVDSetAutoFatalMessaging
 *
 * Turns the automatic fatal-error screen on or off; returns whether it was on.
 */
BOOL DVDSetAutoFatalMessaging(BOOL enable) {
    BOOL enabled;
    BOOL old;

    enabled = OSDisableInterrupts();

    old = (FatalFunc != NULL);

    if (enable) {
        FatalFunc = __DVDShowFatalMessage;
    } else {
        FatalFunc = NULL;
    }

    OSRestoreInterrupts(enabled);
    return old;
} // 0x80225570

/*
 * __DVDGetAutoFatalMessaging
 */
BOOL __DVDGetAutoFatalMessaging(void) {
    return FatalFunc != NULL;
} // 0x802255D0

/*
 * __DVDPrintFatalMessage
 *
 * Shows the fatal-error screen when automatic messaging is on.
 */
void __DVDPrintFatalMessage(void) {
    if (FatalFunc != NULL) {
        FatalFunc();
    }
} // 0x802255F0
