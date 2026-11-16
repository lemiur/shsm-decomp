/* transform_dep: converts a .d file written by `mwcc -MMD` (Wine paths) to normal paths (ports tools/transform_dep.py).
 *
 * On Windows in_wsl() is always false, so a path that does not start with the drive letter z is resolved through
 * $WINEPREFIX/dosdevices (os.path.realpath of the join, as ntpath does it). */
#pragma once
#include "avp/avp.h"

/* The directory os.path.join(wineprefix, "dosdevices"): wineprefix is $WINEPREFIX, else $HOME/.wine. NULL when HOME
 * is unset and WINEPREFIX is not (the Python raises KeyError: 'HOME' at import). */
char *transform_dep_winedevices(AvpArena *a);

/* import_d_file for already decoded text (universal newlines applied). Raises avp_throw(AVP_ERR_OTHER,
 * "IndexError: string index out of range") for an empty path line. */
AvpStr transform_dep_text(AvpArena *a, AvpStr text, const char *winedevices);
