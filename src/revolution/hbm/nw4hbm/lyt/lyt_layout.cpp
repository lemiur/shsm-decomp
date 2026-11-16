// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_layout.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_layout.cpp
// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/nw4hbm/lyt/lyt_layout.cpp
#include "nw4hbm/lyt/layout.h"

#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/bounding.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/group.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/lyt/picture.h"
#include "nw4hbm/lyt/textBox.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/lyt/window.h"
#include "nw4hbm/ut/LinkList.h"
#include "nw4hbm/ut/Rect.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

#include <new>

namespace {

using namespace nw4hbm;
using namespace nw4hbm::lyt;

/*
 * CreateObject
 *
 * Allocates a T from the layout allocator and constructs it, or returns NULL
 * when the allocation fails.
 */
template <class T>
T* CreateObject() {
    void* pMem = Layout::AllocMemory(sizeof(T));

    if (pMem) {
        return new (pMem) T;
    } else {
        return NULL;
    }
} // no copy in retail (inlined)

/*
 * CreateObject
 *
 * As above, constructing the T from `p1`.
 */
template <class T, typename P1>
T* CreateObject(P1 p1) {
    void* pMem = Layout::AllocMemory(sizeof(T));

    if (pMem) {
        return new (pMem) T(p1);
    } else {
        return NULL;
    }
} // no copy in retail (inlined)

/*
 * CreateObject
 *
 * As above, constructing the T from `p1` and `p2`.
 */
template <class T, typename P1, typename P2>
T* CreateObject(P1 p1, P2 p2) {
    void* pMem = Layout::AllocMemory(sizeof(T));

    if (pMem) {
        return new (pMem) T(p1, p2);
    } else {
        return NULL;
    }
} // no copy in retail (inlined)

/*
 * SetTagProcessorImpl
 *
 * Gives every text box in the tree under `pPane` the tag processor.
 */
void SetTagProcessorImpl(Pane* pPane, ut::TagProcessorBase<wchar_t>* pTagProcessor) {
    if (TextBox* pTextBox = ut::DynamicCast<TextBox*>(pPane)) {
        pTextBox->SetTagProcessor(pTagProcessor);
    }

    NW4HBM_RANGE_FOR(it, pPane->GetChildList()) {
        SetTagProcessorImpl(&(*it), pTagProcessor);
    }
} // 0x803C31D0

} // namespace

namespace nw4hbm {
namespace lyt {

MEMAllocator* Layout::mspAllocator;

/*
 * Layout::Layout
 */
Layout::Layout() :
    mpRootPane(NULL),
    mpGroupContainer(NULL),
    mLayoutSize(0.0f, 0.0f),
    mOriginType(OriginType_TopLeft) {} // 0x803C3370

/*
 * Layout::~Layout
 *
 * Destroys the groups, the pane tree (unless the user allocated its root) and
 * the animations the layout created.
 */
Layout::~Layout() {
    if (mpGroupContainer) {
        mpGroupContainer->~GroupContainer();
        FreeMemory(mpGroupContainer);
    }

    if (mpRootPane && !mpRootPane->IsUserAllocated()) {
        mpRootPane->~Pane();
        FreeMemory(mpRootPane);
    }

    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mAnimTransList) {
        __typeof__(it) currIt = it++;

        mAnimTransList.Erase(currIt);
        currIt->~AnimTransform();
        FreeMemory(&(*currIt));
    }
} // 0x803C33B0

/*
 * Layout::Build
 *
 * Builds the pane tree and the groups from the layout file `lytResBuf`. Panes
 * between a start and an end block are children of the pane before the start
 * block; only top-level groups (nest level 1) are kept.
 */
bool Layout::Build(const void* lytResBuf, ResourceAccessor* pResAcsr) {
    const res::BinaryFileHeader* fileHead = static_cast<const res::BinaryFileHeader*>(lytResBuf);

    if (!detail::TestFileHeader(*fileHead, res::SIGNATURE_LAYOUT)) {
        return false;
    }

    ResBlockSet resBlockSet = {};
    resBlockSet.pResAccessor = pResAcsr;

    Pane* pParentPane = NULL;
    Pane* pLastPane = NULL;
    bool bReadRootGroup = false;
    int groupNestLevel = 0;
    const void* dataPtr = detail::ConvertOffsToPtr<void>(lytResBuf, fileHead->headerSize);

    for (int i = 0; i < fileHead->dataBlocks; ++i) {
        const res::DataBlockHeader* pDataBlockHead = static_cast<const res::DataBlockHeader*>(dataPtr);

        switch (detail::GetSignatureInt(pDataBlockHead->kind)) {
        case res::SIGNATURE_LAYOUT_BLOCK: {
            const res::Layout* pResLyt = static_cast<const res::Layout*>(dataPtr);

            mOriginType = static_cast<OriginType>(pResLyt->originType != 0);
            mLayoutSize = pResLyt->layoutSize;
        } break;

        case res::SIGNATURE_TEXTURE_LIST_BLOCK:
            resBlockSet.pTextureList = static_cast<const res::TextureList*>(dataPtr);
            break;

        case res::SIGNATURE_FONT_LIST_BLOCK:
            resBlockSet.pFontList = static_cast<const res::FontList*>(dataPtr);
            break;

        case res::SIGNATURE_MATERIAL_LIST_BLOCK:
            resBlockSet.pMaterialList = static_cast<const res::MaterialList*>(dataPtr);
            break;

        case res::SIGNATURE_PANE_BLOCK:
        case res::SIGNATURE_BOUNDING_BLOCK:
        case res::SIGNATURE_PICTURE_BLOCK:
        case res::SIGNATURE_TEXT_BOX_BLOCK:
        case res::SIGNATURE_WINDOW_BLOCK:
            if (Pane* pPane = BuildPaneObj(detail::GetSignatureInt(pDataBlockHead->kind), dataPtr, resBlockSet)) {
                if (!mpRootPane) {
                    mpRootPane = pPane;
                }

                if (pParentPane) {
                    pParentPane->AppendChild(pPane);
                }

                pLastPane = pPane;
            }
            break;

        case res::SIGNATURE_PANE_START_BLOCK:
            pParentPane = pLastPane;
            break;

        case res::SIGNATURE_PANE_END_BLOCK:
            pLastPane = pParentPane;
            pParentPane = pLastPane->GetParent();
            break;

        case res::SIGNATURE_GROUP_BLOCK:
            if (!bReadRootGroup) {
                bReadRootGroup = true;
                mpGroupContainer = CreateObject<GroupContainer>();
            } else if (mpGroupContainer && groupNestLevel == 1) {
                if (Group* pGroup = CreateObject<Group>(static_cast<const res::Group*>(dataPtr), mpRootPane)) {
                    mpGroupContainer->AppendGroup(pGroup);
                }
            }
            break;

        case res::SIGNATURE_GROUP_BLOCK_START:
            ++groupNestLevel;
            break;

        case res::SIGNATURE_GROUP_BLOCK_END:
            --groupNestLevel;
            break;
        }

        dataPtr = detail::ConvertOffsToPtr<void>(dataPtr, pDataBlockHead->size);
    }

    return true;
} // 0x803C34E0

/*
 * Layout::CreateAnimTransform
 *
 * Creates an animation from the animation file `anmResBuf` and adds it to the
 * layout's list; returns NULL when the file holds no usable animation.
 */
AnimTransform* Layout::CreateAnimTransform(const void* anmResBuf, ResourceAccessor* pResAcsr) {
    const res::BinaryFileHeader* pFileHead = static_cast<const res::BinaryFileHeader*>(anmResBuf);

    if (!detail::TestFileHeader(*pFileHead)) {
        return NULL;
    }

    const res::AnimationBlock* pInfoBlock = NULL;
    const res::DataBlockHeader* pDataBlockHead =
        detail::ConvertOffsToPtr<res::DataBlockHeader>(pFileHead, pFileHead->headerSize);

    AnimTransform* ret = NULL;

    for (int i = 0; i < pFileHead->dataBlocks; ++i) {
        switch (detail::GetSignatureInt(pDataBlockHead->kind)) {
        case res::SIGNATURE_ANIM_INFO_BLOCK:
            switch (detail::GetSignatureInt(pFileHead->signature)) {
            case res::SIGNATURE_ANIMATION:
            case res::AnimationInfo::SIGNATURE_PANE_PAIN_SRT_INFO:
            case res::AnimationInfo::SIGNATURE_PANE_VISIBILITY_INFO:
            case res::AnimationInfo::SIGNATURE_PANE_VERTEX_COLOR_INFO:
            case res::AnimationInfo::SIGNATURE_MATERIAL_COLOR_INFO:
            case res::AnimationInfo::SIGNATURE_MATERIAL_TEXTURE_SRT_INFO:
            case res::AnimationInfo::SIGNATURE_MATERIAL_TEXTURE_PATTERN_INFO:
                if (AnimTransformBasic* pAnimTrans = CreateObject<AnimTransformBasic>()) {
                    pInfoBlock = reinterpret_cast<const res::AnimationBlock*>(pDataBlockHead);

                    pAnimTrans->SetResource(pInfoBlock, pResAcsr);
                    ret = pAnimTrans;
                }
                break;
            }

            if (ret) {
                mAnimTransList.PushBack(ret);
            }
            break;
        }

        pDataBlockHead = detail::ConvertOffsToPtr<res::DataBlockHeader>(pDataBlockHead, pDataBlockHead->size);
    }

    return ret;
} // 0x803C37F0

/*
 * Layout::BindAnimation
 */
void Layout::BindAnimation(AnimTransform* pAnimTrans) {
    if (mpRootPane) {
        mpRootPane->BindAnimation(pAnimTrans, true);
    }
} // 0x803C3940

/*
 * Layout::UnbindAnimation
 */
void Layout::UnbindAnimation(AnimTransform* pAnimTrans) {
    if (mpRootPane) {
        mpRootPane->UnbindAnimation(pAnimTrans, true);
    }
} // 0x803C3970

/*
 * Layout::UnbindAllAnimation
 */
void Layout::UnbindAllAnimation() {
    UnbindAnimation(NULL);
} // 0x803C39A0

/*
 * Layout::SetAnimationEnable
 */
void Layout::SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable) {
    if (mpRootPane) {
        mpRootPane->SetAnimationEnable(pAnimTrans, bEnable, true);
    }
} // 0x803C39C0

/*
 * Layout::CalculateMtx
 */
void Layout::CalculateMtx(const DrawInfo& drawInfo) {
    if (mpRootPane) {
        mpRootPane->CalculateMtx(drawInfo);
    }
} // 0x803C39F0

/*
 * Layout::Draw
 */
void Layout::Draw(const DrawInfo& drawInfo) {
    if (mpRootPane) {
        mpRootPane->Draw(drawInfo);
    }
} // 0x803C3A10

/*
 * Layout::Animate
 */
void Layout::Animate(u32 option) {
    if (mpRootPane) {
        mpRootPane->Animate(option);
    }
} // 0x803C3A30

/*
 * Layout::GetLayoutRect
 *
 * The layout's extent in its own coordinates: centred on the origin, or from
 * the top-left corner.
 */
ut::Rect Layout::GetLayoutRect() const {
    if (mOriginType == OriginType_Center) {
        return ut::Rect(-mLayoutSize.width / 2.0f, mLayoutSize.height / 2.0f, mLayoutSize.width / 2.0f,
                        -mLayoutSize.height / 2.0f);
    } else {
        return ut::Rect(0.0f, 0.0f, mLayoutSize.width, mLayoutSize.height);
    }
} // 0x803C3A50

/*
 * Layout::SetTagProcessor
 */
void Layout::SetTagProcessor(ut::TagProcessorBase<wchar_t>* pTagProcessor) {
    SetTagProcessorImpl(mpRootPane, pTagProcessor);
} // 0x803C3AC0

/*
 * Layout::BuildPaneObj
 *
 * Creates the pane of kind `kind` from its data block; NULL for an unknown
 * kind or a failed allocation.
 */
Pane* Layout::BuildPaneObj(s32 kind, const void* dataPtr, const ResBlockSet& resBlockSet) {
    Pane* pPane;

    switch (kind) {
    case res::SIGNATURE_PANE_BLOCK: {
        const res::Pane* pResPane = static_cast<const res::Pane*>(dataPtr);

        pPane = CreateObject<Pane>(pResPane);
    } break;

    case res::SIGNATURE_PICTURE_BLOCK: {
        const res::Picture* pResPic = static_cast<const res::Picture*>(dataPtr);

        pPane = CreateObject<Picture>(pResPic, resBlockSet);
    } break;

    case res::SIGNATURE_TEXT_BOX_BLOCK: {
        const res::TextBox* pBlock = static_cast<const res::TextBox*>(dataPtr);

        pPane = CreateObject<TextBox>(pBlock, resBlockSet);
    } break;

    case res::SIGNATURE_WINDOW_BLOCK: {
        const res::Window* pBlock = static_cast<const res::Window*>(dataPtr);

        pPane = CreateObject<Window>(pBlock, resBlockSet);
    } break;

    case res::SIGNATURE_BOUNDING_BLOCK: {
        const res::Bounding* pResBounding = static_cast<const res::Bounding*>(dataPtr);

        pPane = CreateObject<Bounding>(pResBounding, resBlockSet);
    } break;

    default:
        pPane = NULL;
        break;
    }

    return pPane;
} // 0x803C3C70

} // namespace lyt
} // namespace nw4hbm
