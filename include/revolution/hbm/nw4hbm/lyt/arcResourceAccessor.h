#ifndef NW4HBM_LYT_ARC_RESOURCE_ACCESSOR_H
#define NW4HBM_LYT_ARC_RESOURCE_ACCESSOR_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/arcResourceAccessor.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_arcResourceAccessor.h

#include <revolution/types.h>
#include <revolution/arc.h>
#include "nw4hbm/lyt/resourceAccessor.h"
#include "nw4hbm/ut/LinkList.h"

namespace nw4hbm {
namespace ut {
class Font;
} // namespace ut

namespace lyt {

/*
 * FontRefLink
 *
 * Registers a font under a name with an accessor's font list.
 */
class FontRefLink {
public:
    typedef ut::LinkList<FontRefLink, 0> LinkList;

    FontRefLink();

    const char* GetFontName() const { return mFontName; }
    ut::Font* GetFont() const { return mFont; }

    void Set(const char* name, ut::Font* font);

    static const u32 FONT_NAME_SIZE = 128;

private:
    ut::LinkListNode mLink;          // 0x00
    char mFontName[FONT_NAME_SIZE];  // 0x08
    ut::Font* mFont;                 // 0x88
}; // size 0x8C

/*
 * ArcResourceAccessor
 *
 * Resource accessor over a single U8 archive: resources are looked up under a
 * root directory, by type directory and name. Fonts come from a registered list.
 */
class ArcResourceAccessor : public ResourceAccessor {
public:
    ArcResourceAccessor();

    virtual void* GetResource(u32 resType, const char* name, u32* pSize);
    virtual ut::Font* GetFont(const char* name);

    bool Attach(void* archiveStart, const char* resourceRootDirectory);
    void* Detach();

    void RegistFont(FontRefLink* link);
    void UnregistFont(FontRefLink* link);

    static const u32 RES_ROOT_DIR_SIZE = 128;

private:
    ARCHandle mArcHandle;                 // 0x04
    void* mArcBuf;                        // 0x20
    FontRefLink::LinkList mFontList;      // 0x24
    char mResRootDir[RES_ROOT_DIR_SIZE];  // 0x30
}; // size 0xB0

} // namespace lyt
} // namespace nw4hbm

#endif
