/* Small text helpers the shsm_* modules share: Python's regex \s, shlex.split. */
#pragma once
#include "avp/str.h"

/* Byte length of the whitespace code point at s[i] (Python re's \s on str, i.e. str.isspace), 0 when none. */
size_t shsm_ws_len(AvpStr s, size_t i);
/* The offset after the run of whitespace starting at i (\s*). */
size_t shsm_skip_ws(AvpStr s, size_t i);
/* shlex.split(s, posix=True) (comments off): true with the words in *out (strings of `a`), or false with *err set to
 * shlex's ValueError text ("No closing quotation", "No escaped character"). */
bool shsm_shlex_split(AvpArena *a, AvpStr s, AvpStrVec *out, const char **err);
