#ifndef NW4HBM_LYT_LAYOUT_H
#define NW4HBM_LYT_LAYOUT_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/layout.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_layout.h

#include <revolution/types.h>
#include <revolution/mem/allocator.h>
#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"

namespace nw4hbm {
namespace ut {
struct Rect;
template <typename CharT> class TagProcessorBase;
} // namespace ut

namespace lyt {

class DrawInfo;
class GroupContainer;
class Pane;
class ResourceAccessor;

namespace res {

// file and data-block kinds, the four characters as a big-endian word
static const u32 SIGNATURE_LAYOUT = 'RLYT';
static const u32 SIGNATURE_LAYOUT_BLOCK = 'lyt1';
static const u32 SIGNATURE_TEXTURE_LIST_BLOCK = 'txl1';
static const u32 SIGNATURE_FONT_LIST_BLOCK = 'fnl1';
static const u32 SIGNATURE_MATERIAL_LIST_BLOCK = 'mat1';

/*
 * res::Layout
 *
 * Layout-file block with the layout's origin and size ("lyt1").
 */
struct Layout {
    DataBlockHeader blockHeader; // 0x00
    u8 originType;               // 0x08 Layout::OriginType_et
    u8 padding[3];               // 0x09 // pad: SDK res::Layout.padding
    Size layoutSize;             // 0x0C
}; // size 0x14

/*
 * TextureList
 *
 * Layout-file block that lists the textures a layout uses ("txl1").
 */
struct TextureList {
    DataBlockHeader blockHeader; // 0x00
    u16 texNum;                  // 0x08
    u8 padding[2];               // 0x0A // pad: SDK res::TextureList.padding
}; // size 0x0C

/*
 * FontList
 *
 * Layout-file block that lists the fonts a layout uses ("fnl1").
 */
struct FontList {
    DataBlockHeader blockHeader; // 0x00
    u16 fontNum;                 // 0x08
    u8 padding[2];               // 0x0A // pad: SDK res::FontList.padding
}; // size 0x0C

/*
 * MaterialList
 *
 * Layout-file block that lists the materials a layout uses ("mat1"); the
 * offsets of the materials follow it.
 */
struct MaterialList {
    DataBlockHeader blockHeader; // 0x00
    u16 materialNum;             // 0x08
    u8 padding[2];               // 0x0A // pad: SDK res::MaterialList.padding
}; // size 0x0C

} // namespace res

/*
 * ResBlockSet
 *
 * The resource blocks of a layout file that panes are built from.
 */
struct ResBlockSet {
    const res::TextureList* pTextureList;   // 0x00
    const res::FontList* pFontList;         // 0x04
    const res::MaterialList* pMaterialList; // 0x08
    ResourceAccessor* pResAccessor;         // 0x0C
}; // size 0x10

/*
 * Layout
 *
 * A whole layout: the pane tree and groups built from a layout file, and the
 * animations created for it. Every object a layout builds is allocated from
 * the one allocator set with SetAllocator.
 */
class Layout {
public:
    typedef u8 OriginType;
    enum OriginType_et {
        OriginType_TopLeft,
        OriginType_Center
    };

    Layout();
    virtual ~Layout();

    virtual bool Build(const void* lytResBuf, ResourceAccessor* pResAcsr);
    virtual AnimTransform* CreateAnimTransform(const void* anmResBuf, ResourceAccessor* pResAcsr);
    virtual void BindAnimation(AnimTransform* pAnimTrans);
    virtual void UnbindAnimation(AnimTransform* pAnimTrans);
    virtual void UnbindAllAnimation();
    virtual void SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable);
    virtual void CalculateMtx(const DrawInfo& drawInfo);
    virtual void Draw(const DrawInfo& drawInfo);
    virtual void Animate(u32 option);
    virtual void SetTagProcessor(ut::TagProcessorBase<wchar_t>* pTagProcessor);

    Pane* GetRootPane() const { return mpRootPane; }
    GroupContainer* GetGroupContainer() const { return mpGroupContainer; }

    ut::Rect GetLayoutRect() const;

    static MEMAllocator* GetAllocator() { return mspAllocator; }
    static void SetAllocator(MEMAllocator* pAllocator) { mspAllocator = pAllocator; }

    static void* AllocMemory(u32 size) { return MEMAllocFromAllocator(mspAllocator, size); }
    static void FreeMemory(void* mem) { MEMFreeToAllocator(mspAllocator, mem); }

    static Pane* BuildPaneObj(s32 kind, const void* dataPtr, const ResBlockSet& resBlockSet);

private:
    static MEMAllocator* mspAllocator;

    AnimTransform::LinkList mAnimTransList; // 0x04 animations created by CreateAnimTransform
    Pane* mpRootPane;                       // 0x10
    GroupContainer* mpGroupContainer;       // 0x14
    Size mLayoutSize;                       // 0x18
    OriginType mOriginType;                 // 0x20 OriginType_et
}; // size 0x24

} // namespace lyt
} // namespace nw4hbm

#endif
