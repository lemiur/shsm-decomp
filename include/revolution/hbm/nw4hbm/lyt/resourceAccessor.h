#ifndef NW4HBM_LYT_RESOURCE_ACCESSOR_H
#define NW4HBM_LYT_RESOURCE_ACCESSOR_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/resourceAccessor.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_resourceAccessor.h

#include <revolution/types.h>

namespace nw4hbm {
namespace ut {
class Font;
} // namespace ut

namespace lyt {

/*
 * ResourceAccessor
 *
 * Abstract source of the layout's resources (textures, fonts), looked up by
 * type and name.
 */
class ResourceAccessor {
public:
    ResourceAccessor();
    virtual ~ResourceAccessor();

    virtual void* GetResource(u32 resType, const char* name, u32* pSize) = 0;
    virtual ut::Font* GetFont(const char* name);

    // resource types, the four characters of the kind as a big-endian word
    static const u32 SIGNATURE_FONT = 'font';
    static const u32 SIGNATURE_TEXTURE_IMAGE = 'timg';
};

} // namespace lyt
} // namespace nw4hbm

#endif
