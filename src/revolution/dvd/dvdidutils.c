// Reference: zeldaret/tp libs/revolution/src/dvd/dvdidutils.c, with xbret/xenoblade
// libs/RVL_SDK/src/revolution/dvd/dvdidutils.c (same SDK build).
// The debug-only ASSERTMSGLINE checks (and the strnlen helper only they use) compile to nothing in this release
// build and are left out (see dvdfs.c).
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <string.h>

/*
 * DVDCompareDiskID
 *
 * Compares two disk IDs field by field. An empty game name, or a disk or version number of 0xFF, is a wildcard that
 * matches anything; the company code must be present in both.
 */
int DVDCompareDiskID(const DVDDiskID* id1, const DVDDiskID* id2) {
    if (id1->gameName[0] != 0 && id2->gameName[0] != 0 && strncmp(id1->gameName, id2->gameName, 4) != 0) {
        return 0;
    }

    if (id1->company[0] == 0 || id2->company[0] == 0 || strncmp(id1->company, id2->company, 2) != 0) {
        return 0;
    }

    if (id1->diskNumber != 0xFF && id2->diskNumber != 0xFF && id1->diskNumber != id2->diskNumber) {
        return 0;
    }

    if (id1->gameVersion != 0xFF && id2->gameVersion != 0xFF && id1->gameVersion != id2->gameVersion) {
        return 0;
    }

    return 1;
} // 0x802253B0

/*
 * DVDGenerateDiskID
 *
 * Fills in a disk ID from its parts; a NULL game or company leaves that field zeroed.
 */
DVDDiskID* DVDGenerateDiskID(DVDDiskID* id, const char* game, const char* company, u8 diskNum, u8 version) {
    memset(id, 0, sizeof(DVDDiskID));

    if (game != NULL) {
        strncpy(id->gameName, game, 4);
    }

    if (company != NULL) {
        strncpy(id->company, company, 2);
    }

    id->diskNumber = diskNum;
    id->gameVersion = version;
    return id;
} // no copy in retail (dead-stripped)
