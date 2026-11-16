/* shsm_placeholder: the body `unit.exe new` writes for a function it has not reconstructed, and the recogniser the
 * regression checks of merge_lane and gate share (one definition: the generator and the checks cannot drift). */
#pragma once
#include "avp/str.h"

/* The one line between the braces of a generated stub (unit_stub.c emits `<signature> {`, this line, `}`, with a
 * trailing `// 0x8XXXXXXX` comment on the closing line in non-game units). */
#define SHSM_PLACEHOLDER_BODY_LINE "    // TODO"

/* True when `func_text` (a function definition from its signature to its closing brace, a trailing line comment after
 * the brace allowed) has exactly the generator's body: the text between its first `{` and its last `}` is only the
 * `// TODO` comment line. Any other statement or comment, or no braces, is false. */
bool shsm_is_placeholder_body(AvpStr func_text);
