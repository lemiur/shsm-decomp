// Reference: doldecomp/ogws src/revolution/WUD/debug_msg.c (zeldaret/tp libs/revolution/src/wud/debug_msg.c has the
// same function with an unread `va_list` local)
#include <revolution/wud/__wud.h>

/*
 * WUD_DEBUGPrint
 *
 * Debug print hook of the WUD library; the release build discards the message.
 */
void WUD_DEBUGPrint(const char* fmt, ...) {
#pragma unused(fmt)
} // 0x80290EB0
