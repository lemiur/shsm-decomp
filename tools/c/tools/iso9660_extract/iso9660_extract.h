/* A port of tools/scripts/iso9660_extract.py: a minimal ISO9660 lister and extractor. */
#pragma once
#include "avp/avp.h"

int iso9660_main(int argc, char **argv);

/* parse_record's name rules on the raw identifier bytes: "." and ".." for the one-byte 0 and 1 identifiers, else the
 * latin-1 text without a trailing ";<digits>" version, as UTF-8. */
AvpStr iso9660_clean_name(AvpArena *a, const u8 *name, size_t n);
/* a latin-1 text as UTF-8 (what Python prints for the decoded str). */
AvpStr iso9660_latin1_utf8(AvpArena *a, const u8 *p, size_t n);
