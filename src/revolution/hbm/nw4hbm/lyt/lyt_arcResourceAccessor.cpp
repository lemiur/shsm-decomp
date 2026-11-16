// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_arcResourceAccessor.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_arcResourceAccessor.cpp
#include "nw4hbm/lyt/arcResourceAccessor.h"

#include <extras.h>
#include <string.h>

namespace {

/*
 * FindNameResource
 *
 * Searches the current directory and its subdirectories for a file whose name
 * equals `resName` (ignoring case); returns its entry number, or -1.
 */
s32 FindNameResource(ARCHandle* arcHandle, const char* resName) {
    s32 entryNum = -1;

    ARCDir dir;
    ARCOpenDir(arcHandle, ".", &dir);

    ARCDirEntry dirEntry;
    while (ARCReadDir(&dir, &dirEntry)) {
        if (dirEntry.isDir) {
            ARCChangeDir(arcHandle, dirEntry.name);
            entryNum = FindNameResource(arcHandle, resName);
            ARCChangeDir(arcHandle, "..");

            if (entryNum != -1) {
                break;
            }
        } else if (stricmp(resName, dirEntry.name) == 0) {
            entryNum = dirEntry.entryNum;
            break;
        }
    }

    ARCCloseDir(&dir);
    return entryNum;
} // 0x803C1A10

/*
 * GetResourceSub
 *
 * Finds a resource in the archive under `resRootDir`. A zero `resType` searches
 * the whole tree by name; otherwise the type's four characters name the
 * directory that holds the resource. Returns the resource's address in the
 * archive and stores its size in `pSize` when given.
 */
void* GetResourceSub(ARCHandle* arcHandle, const char* resRootDir, u32 resType, const char* name,
                     u32* pSize) {
    s32 entryNum = -1;

    if (ARCConvertPathToEntrynum(arcHandle, resRootDir) != -1 && ARCChangeDir(arcHandle, resRootDir)) {
        if (resType == 0) {
            entryNum = FindNameResource(arcHandle, name);
        } else {
            char resTypeStr[sizeof(u32) + 1];
            resTypeStr[0] = resType >> 24;
            resTypeStr[1] = resType >> 16;
            resTypeStr[2] = resType >> 8;
            resTypeStr[3] = resType;
            resTypeStr[4] = '\0';

            if (ARCConvertPathToEntrynum(arcHandle, resTypeStr) != -1 &&
                ARCChangeDir(arcHandle, resTypeStr)) {
                entryNum = ARCConvertPathToEntrynum(arcHandle, name);
                ARCChangeDir(arcHandle, "..");
            }
        }

        ARCChangeDir(arcHandle, "..");
    }

    if (entryNum != -1) {
        ARCFileInfo arcFileInfo;
        ARCFastOpen(arcHandle, entryNum, &arcFileInfo);

        void* resource = ARCGetStartAddrInMem(&arcFileInfo);

        if (pSize != NULL) {
            *pSize = ARCGetLength(&arcFileInfo);
        }

        ARCClose(&arcFileInfo);
        return resource;
    }

    return NULL;
} // 0x803C1AF0

} // namespace

namespace nw4hbm {
namespace lyt {

namespace detail {

/*
 * FindFont
 *
 * Returns the font registered under `name`, or NULL.
 */
static ut::Font* FindFont(FontRefLink::LinkList* fontList, const char* name) {
    NW4HBM_RANGE_FOR(it, *fontList) {
        if (strcmp(name, it->GetFontName()) == 0) {
            return it->GetFont();
        }
    }

    return NULL;
}

} // namespace detail

/*
 * ArcResourceAccessor::ArcResourceAccessor
 */
ArcResourceAccessor::ArcResourceAccessor() : mArcBuf(NULL) {} // 0x803C1C40

/*
 * ArcResourceAccessor::Attach
 *
 * Opens the archive at `archiveStart` and remembers the root directory that
 * resource lookups start from.
 */
bool ArcResourceAccessor::Attach(void* archiveStart, const char* resourceRootDirectory) {
    BOOL success = ARCInitHandle(archiveStart, &mArcHandle);

    if (!success) {
        return false;
    }

    mArcBuf = archiveStart;

    strncpy(mResRootDir, resourceRootDirectory, RES_ROOT_DIR_SIZE - 1);
    mResRootDir[RES_ROOT_DIR_SIZE - 1] = '\0';

    return true;
} // 0x803C1CA0

/*
 * ArcResourceAccessor::GetResource
 */
void* ArcResourceAccessor::GetResource(u32 resType, const char* name, u32* pSize) {
    return GetResourceSub(&mArcHandle, mResRootDir, resType, name, pSize);
} // 0x803C1D20

/*
 * ArcResourceAccessor::GetFont
 */
ut::Font* ArcResourceAccessor::GetFont(const char* name) {
    return detail::FindFont(&mFontList, name);
} // 0x803C1D40

} // namespace lyt
} // namespace nw4hbm
