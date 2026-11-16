/* wdbg.exe iro-scan (wdbg_iro.c). */
#pragma once
#include "avp/avp.h"

/* argv after "iro-scan"; self is this executable (each unit is one run of its single-run path). Returns the exit code. */
int wdbg_iro_scan(AvpArena *a, const char *self, int argc, char **argv);
