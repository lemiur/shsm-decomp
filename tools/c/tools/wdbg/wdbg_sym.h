/* D.sym(addr) of tools/mwcc_trace/wdbg.py: the compiler address named from the NB11 debug info of the compiler the
 * run uses (WdbgDebug.exe: the unit's mw_version; the Python always read the default compiler,
 * tools/compilers/Wii/1.0/mwcceppc.exe, symbols.DEFAULT_EXE), or hex(addr) when the table cannot be loaded or has no
 * name for it. */
#pragma once
#include "wdbg_debug.h"

AvpStr wdbg_sym(WdbgDebug *d, AvpArena *a, i64 addr);
/* addr("Name") of the spec language: the address of the function called name in that compiler; false when its table
 * cannot be loaded or has no function of exactly that name. */
bool wdbg_sym_lookup(WdbgDebug *d, AvpArena *a, AvpStr name, i64 *addr);
