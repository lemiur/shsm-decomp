// Reference: zeldaret/tp libs/revolution/src/wpad/wpad_debug_msg.c, doldecomp/ogws src/revolution/WPAD/debug_msg.c
// Unit name from the references (wpad_debug_msg.c in tp, debug_msg.c in ogws); unconfirmed for this binary.
#include <revolution/wpad/__wpad.h>

// Release builds compile the debug printer to nothing but its varargs prologue.
void DEBUGPrint(const char* fmt, ...) {
#pragma unused(fmt)
} // 0x8028A470
