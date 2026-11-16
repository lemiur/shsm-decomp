/* download_tool: downloads a build tool from its release URL (ports tools/download_tool.py).
 *
 * The URL of each tool is built from its name and tag exactly as the Python builds it; a zip is unpacked as
 * zipfile.ZipFile.extractall does (names made relative, Windows-illegal characters replaced, CRC checked), anything else
 * is written to the output file. */
#pragma once
#include "avp/avp.h"

/* An exception the Python would raise: class name and str(). */
typedef struct {
    const char *cls;
    char text[600];
} DtErr;

/* The URL for `tool` and `tag` on a platform as platform.uname() reports it (system "windows", machine "amd64"). NULL
 * for a name that is not in the tool table (the Python's KeyError). */
char *dt_tool_url(AvpArena *a, const char *tool, const char *tag, const char *system, const char *machine);
/* platform.uname().machine on this machine, lower case ("amd64"). */
const char *dt_machine(AvpArena *a);

/* zipfile.ZipFile(BytesIO(data)).extractall(dest). False with *err set (BadZipFile, NotImplementedError, OSError...). */
bool dt_zip_extract_all(AvpArena *a, const u8 *data, size_t n, const char *dest, DtErr *err);

/* The path under the destination a member is written to (ZipFile._extract_member's arcname rules on Windows); NULL
 * (empty) for a member with no name left; dt_zip_extract_all then raises ValueError("Empty filename."). */
char *dt_zip_arcname(AvpArena *a, AvpStr filename);
