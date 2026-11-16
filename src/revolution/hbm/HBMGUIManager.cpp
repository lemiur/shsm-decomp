// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/HBMGUIManager.cpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMGUIManager.cpp
#include "HBMGUIManager.h"

#include <revolution/gx.h>

#include <new>

#include "nw4hbm/lyt/bounding.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/picture.h"
#include "nw4hbm/lyt/window.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

namespace homebutton {
namespace gui {

u32 PaneManager::suIDCounter;

static void drawLine_(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u8 uWidth, GXColor& rColor);

static bool is_visible(nw4hbm::lyt::Pane* pPane);

/*
 * drawLine_
 *
 * Draws the debug outline of one pane edge: a red (or blue, for the pointed
 * pane) line in the menu's own vertex format.
 */
static void drawLine_(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u8 uWidth, GXColor& rColor) {
    // R36: an unused static const in the reference; its storage is retail's 1.0f at .rodata 0x803DE934.
    static const f32 cubeScale = 1.0f;

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GXSetCullMode(GX_CULL_NONE);

    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, false, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);

    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);

    Mtx modelMtx;
    PSMTXTrans(modelMtx, 0.0f, 0.0f, 0.0f);
    GXLoadPosMtxImm(modelMtx, 0);

    GXSetLineWidth(uWidth, GX_TO_ZERO);

    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(x0, y0, z);
    GXColor1u32(*reinterpret_cast<u32*>(&rColor));
    GXPosition3f32(x1, y1, z);
    GXColor1u32(*reinterpret_cast<u32*>(&rColor));
    GXEnd();
} // 0x803B4ED0

/*
 * Component::update
 *
 * Routes one channel's pointer state to the component and reports the events
 * a point, a move and a release produce. Returns whether the pointer is on the
 * component.
 */
bool Component::update(int i, f32 x, f32 y, u32, u32, u32, void* pData) {
    bool bTouched = false;

    if (!isVisible()) {
        /* nothing */
    } else {
        if (contain(x, y)) {
            if (isPointed(i)) {
                onMove(x, y);
                mpManager->onEvent(getID(), 3, pData);
            } else {
                setPointed(i, true);
                onPoint();
                mpManager->onEvent(getID(), 1, pData);
            }

            bTouched = true;
        } else {
            if (isPointed(i)) {
                setPointed(i, false);
                offPoint();
                mpManager->onEvent(getID(), 2, pData);
            }
        }
    }

    return bTouched;
} // 0x803B50A0

/*
 * Manager::~Manager
 *
 * Frees every component in the ID list.
 */
Manager::~Manager() {
    void* p = nw4hbm::ut::List_GetFirst(&mIDToComponent);

    for (; p; p = nw4hbm::ut::List_GetFirst(&mIDToComponent)) {
        nw4hbm::ut::List_Remove(&mIDToComponent, p);

        if (mpAllocator) {
            MEMFreeToAllocator(mpAllocator, p);
        } else {
            delete static_cast<IDToComponent*>(p);
        }
    }
} // 0x803B5350

/*
 * Manager::init
 *
 * Initialises the components in ID order.
 */
void Manager::init() {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, i));

        p->mpComponent->init();
    }
} // 0x803B5410

/*
 * Manager::addComponent
 *
 * Gives the component a manager and appends its ID entry.
 */
void Manager::addComponent(Component* pComponent) {
    u32 uID = pComponent->getID();
    pComponent->setManager(this);

    if (mpAllocator) {
        void* p = MEMAllocFromAllocator(mpAllocator, sizeof(IDToComponent));
        nw4hbm::ut::List_Append(&mIDToComponent, new (p) IDToComponent(uID, pComponent));
    } else {
        nw4hbm::ut::List_Append(&mIDToComponent, new IDToComponent(uID, pComponent));
    }
} // 0x803B54B0

/*
 * Manager::delComponent
 *
 * Removes the component's ID entry and frees it.
 */
void Manager::delComponent(Component* pComponent) {
    IDToComponent* p = static_cast<IDToComponent*>(nw4hbm::ut::List_GetNext(&mIDToComponent, NULL));

    while (p) {
        if (p->mpComponent == pComponent) {
            break;
        }

        p = static_cast<IDToComponent*>(nw4hbm::ut::List_GetNext(&mIDToComponent, p));
    }

    nw4hbm::ut::List_Remove(&mIDToComponent, p);

    if (mpAllocator) {
        MEMFreeToAllocator(mpAllocator, p);
    } else {
        delete p;
    }
} // 0x803B5590

/*
 * Manager::getComponent
 *
 * The component with the given ID (the ID is its index in the list).
 */
Component* Manager::getComponent(u32 uID) {
    const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, uID));

    return p->mpComponent;
} // 0x803B5630

/*
 * Manager::update
 *
 * Updates every component with the channel's pointer state. The last component
 * the pointer is on takes the channel's trigger and release events.
 */
bool Manager::update(int i, f32 x, f32 y, u32 uTrigFlag, u32 uHoldFlag, u32 uReleaseFlag, void* pData) {
    bool bTouched = false;
    Component* pLastContainedComponent = NULL;

    for (u32 n = 0; n < nw4hbm::ut::List_GetSize(&mIDToComponent); n++) {
        const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, n));

        if (p->mpComponent->update(i, x, y, uTrigFlag, uHoldFlag, uReleaseFlag, pData)) {
            if (p->mpComponent->isTriggerTarger()) {
                pLastContainedComponent = p->mpComponent;
            }

            bTouched = true;
        }
    }

    if (pLastContainedComponent) {
        if (uTrigFlag) {
            Vec pos;
            pLastContainedComponent->onTrig(uTrigFlag, pos);

            onEvent(pLastContainedComponent->getID(), 0, pData);
        }

        if (uReleaseFlag) {
            Vec pos;
            pLastContainedComponent->onTrig(uReleaseFlag, pos);

            onEvent(pLastContainedComponent->getID(), 5, pData);
        }
    }

    return bTouched;
} // 0x803B5660

/*
 * Manager::calc
 *
 * Runs every component's per-frame calculation.
 */
void Manager::calc() {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, i));

        p->mpComponent->calc();
    }
} // 0x803B5850

/*
 * Manager::draw
 *
 * Draws every component.
 */
void Manager::draw() {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, i));

        p->mpComponent->draw();
    }
} // 0x803B58D0

/*
 * Manager::setAllComponentTriggerTarget
 *
 * Makes every component (not) take the channel's trigger and release events.
 */
void Manager::setAllComponentTriggerTarget(bool b) {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        const IDToComponent* p = static_cast<const IDToComponent*>(nw4hbm::ut::List_GetNth(&mIDToComponent, i));

        p->mpComponent->setTriggerTarget(b);
    }
} // 0x803B5950

/*
 * PaneManager::~PaneManager
 *
 * Frees every pane component and its pane mapping.
 */
PaneManager::~PaneManager() {
    PaneToComponent* pPaneToComponent = static_cast<PaneToComponent*>(nw4hbm::ut::List_GetFirst(&mPaneToComponent));

    for (; pPaneToComponent;
         pPaneToComponent = static_cast<PaneToComponent*>(nw4hbm::ut::List_GetFirst(&mPaneToComponent))) {
        nw4hbm::ut::List_Remove(&mPaneToComponent, pPaneToComponent);

        if (mpAllocator) {
            MEMFreeToAllocator(mpAllocator, pPaneToComponent->mpComponent);
            MEMFreeToAllocator(mpAllocator, pPaneToComponent);
        } else {
            delete pPaneToComponent->mpComponent;
            delete pPaneToComponent;
        }
    }
} // 0x803B59E0

/*
 * PaneManager::createLayoutScene
 *
 * Builds the components of a layout scene, numbering them from zero.
 */
void PaneManager::createLayoutScene(const nw4hbm::lyt::Layout& rLayout) {
    suIDCounter = 0;

    nw4hbm::lyt::Pane* pRootPane = rLayout.GetRootPane();

    walkInChildren(pRootPane->GetChildList());
} // 0x803B5BC0

/*
 * PaneManager::addLayoutScene
 *
 * Adds the components of another layout scene without renumbering.
 */
void PaneManager::addLayoutScene(const nw4hbm::lyt::Layout& rLayout) {
    nw4hbm::lyt::Pane* pRootPane = rLayout.GetRootPane();

    walkInChildren(pRootPane->GetChildList());
} // 0x803B5BF0

/*
 * PaneManager::walkInChildren
 *
 * Creates a PaneComponent for every pane in the list and, recursively, in its
 * children. Picture and Window panes take the channel's trigger events.
 */
void PaneManager::walkInChildren(nw4hbm::lyt::PaneList& rPaneList) {
    for (nw4hbm::lyt::PaneList::Iterator it = rPaneList.GetBeginIter(); it != rPaneList.GetEndIter(); it++) {
        PaneComponent* pPaneComponent = NULL;
        PaneToComponent* pPaneToComponent = NULL;

        if (mpAllocator) {
            void* p1 = MEMAllocFromAllocator(mpAllocator, sizeof(*pPaneComponent));
            void* p2 = MEMAllocFromAllocator(mpAllocator, sizeof(*pPaneToComponent));

            pPaneComponent = new (p1) PaneComponent(suIDCounter);
            pPaneToComponent = new (p2) PaneToComponent(&(*it), pPaneComponent);
        } else {
            pPaneComponent = new PaneComponent(suIDCounter);
            pPaneToComponent = new PaneToComponent(&(*it), pPaneComponent);
        }

        nw4hbm::ut::List_Append(&mPaneToComponent, pPaneToComponent);
        suIDCounter++;

        pPaneComponent->setPane(&(*it));

        if (nw4hbm::ut::DynamicCast<nw4hbm::lyt::Picture*>(&(*it))) {
            pPaneComponent->setTriggerTarget(true);
        }

        if (nw4hbm::ut::DynamicCast<nw4hbm::lyt::Window*>(&(*it))) {
            pPaneComponent->setTriggerTarget(true);
        }

        addComponent(pPaneComponent);
        walkInChildren(it->GetChildList());
    }
} // 0x803B5C10

/*
 * PaneManager::delLayoutScene
 *
 * Removes the components of a layout scene.
 */
void PaneManager::delLayoutScene(const nw4hbm::lyt::Layout& rLayout) {
    nw4hbm::lyt::Pane* pRootPane = rLayout.GetRootPane();

    walkInChildrenDel(pRootPane->GetChildList());
} // 0x803B5ED0

/*
 * PaneManager::walkInChildrenDel
 *
 * Removes the component of every pane in the list and, recursively, in its
 * children.
 */
void PaneManager::walkInChildrenDel(nw4hbm::lyt::PaneList& rPaneList) {
    for (nw4hbm::lyt::PaneList::Iterator it = rPaneList.GetBeginIter(); it != rPaneList.GetEndIter(); it++) {
        PaneToComponent* pPaneToComponent =
            static_cast<PaneToComponent*>(nw4hbm::ut::List_GetNext(&mPaneToComponent, NULL));

        while (pPaneToComponent) {
            if (pPaneToComponent->mpPane == &(*it)) {
                break;
            }

            pPaneToComponent =
                static_cast<PaneToComponent*>(nw4hbm::ut::List_GetNext(&mPaneToComponent, pPaneToComponent));
        }

        delComponent(pPaneToComponent->mpComponent);
        nw4hbm::ut::List_Remove(&mPaneToComponent, pPaneToComponent);
        suIDCounter--;

        if (mpAllocator) {
            MEMFreeToAllocator(mpAllocator, pPaneToComponent->mpComponent);
            MEMFreeToAllocator(mpAllocator, pPaneToComponent);
        } else {
            delete pPaneToComponent->mpComponent;
            delete pPaneToComponent;
        }

        walkInChildrenDel(it->GetChildList());
    }
} // 0x803B5EF0

/*
 * PaneManager::getPaneComponentByPane
 *
 * The component of the given pane, or NULL when the pane is not in the scene.
 */
PaneComponent* PaneManager::getPaneComponentByPane(nw4hbm::lyt::Pane* pPane) {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        PaneToComponent* p = static_cast<PaneToComponent*>(nw4hbm::ut::List_GetNth(&mPaneToComponent, i));

        if (p->mpPane == pPane) {
            return p->mpComponent;
        }
    }

    return NULL;
} // 0x803B6010

/*
 * PaneManager::setAllBoundingBoxComponentTriggerTarget
 *
 * Makes every Bounding pane's component (not) take the channel's trigger
 * events.
 *
 * The pragma below is an UNPROVEN codegen control. mscharged's R4QK01
 * HBMGUIManager.cpp does not have it; the only source that carries it is
 * sdk_2009-12-11, wrapped there in `#if defined(NDEBUG)` and marked
 * `TODO(FAKE)`, with the note "also works as #pragma opt_propagation off, but I
 * think it's probably this quirk". With precedence propagation on, the compiler
 * duplicates the loop's first test; either pragma gives retail's shared test at
 * the loop bottom, and the bytes are identical. Kept `opt_propagation off`
 * because it is the narrower switch sdk_2009's author named as the equivalent
 * form; `#pragma ppc_iro_level 0` (the spelling in that reference) gives the
 * same bytes. What would make it authentic: evidence that the SDK built this
 * unit with that setting (a per-object flag, or the pragma in a reference that
 * is not marked FAKE).
 *
 * RULE-EXCEPTION: R17, unproven compiler-state lever; the pragma is the only
 * source-level control that reproduces retail's loop block layout (see
 * docs/exceptions.md, "HBM rulings (sdk-hbm-15)").
 */
#pragma push
#pragma opt_propagation off
void PaneManager::setAllBoundingBoxComponentTriggerTarget(bool b) {
    for (u32 i = 0; i < nw4hbm::ut::List_GetSize(&mIDToComponent); i++) {
        PaneToComponent* p = static_cast<PaneToComponent*>(nw4hbm::ut::List_GetNth(&mPaneToComponent, i));

        if (nw4hbm::ut::DynamicCast<nw4hbm::lyt::Bounding*>(p->mpPane)) {
            p->mpComponent->setTriggerTarget(b);
        }
    }
} // 0x803B6090
#pragma pop

/*
 * PaneComponent::contain
 *
 * Whether the given screen position, taken into the pane's local space, is
 * inside the pane's rectangle.
 */
bool PaneComponent::contain(f32 x_, f32 y_) {
    if (!mpManager) {
        return false;
    }

    const nw4hbm::lyt::DrawInfo* pDrawInfo = static_cast<PaneManager*>(mpManager)->getDrawInfo();

    if (!pDrawInfo) {
        return false;
    }

    nw4hbm::math::MTX34 invGlbMtx;
    PSMTXInverse(mpPane->GetGlobalMtx(), invGlbMtx);

    nw4hbm::math::VEC3 lclPos;
    PSMTXMultVec(invGlbMtx, nw4hbm::math::VEC3(x_, y_, 0.0f), lclPos);

    nw4hbm::ut::Rect rect = mpPane->GetPaneRect(*pDrawInfo);

    if (rect.left <= lclPos.x && lclPos.x <= rect.right && rect.bottom <= lclPos.y && lclPos.y <= rect.top) {
        return true;
    } else {
        return false;
    }
} // 0x803B6160

/*
 * PaneComponent::draw
 *
 * Draws the pane's debug outline, following the pane's position and size. The
 * pointed pane's outline is blue.
 */
void PaneComponent::draw() {
    const nw4hbm::lyt::DrawInfo* pDrawInfo = static_cast<PaneManager*>(mpManager)->getDrawInfo();

    if (!pDrawInfo) {
        return;
    }

    // R36: the reference's unread call (both references keep it; a stripped debug outline used the
    // translate). The accessor is inline, so it is byte-neutral.
    (void)mpPane->GetTranslate();

    nw4hbm::lyt::Size size = mpPane->GetSize();

    const nw4hbm::math::MTX34& gmtx = mpPane->GetGlobalMtx();

    f32 x = gmtx.mtx[0][3];
    f32 y = gmtx.mtx[1][3];

    GXColor color = {0xff, 0x00, 0x00, 0xff}; // red

    if (mabPointed[0]) {
        color.r = 0x00;
        color.b = 0xff; // now blue
    }

    // start at top left, go clockwise
    drawLine_(x - size.width / 2.0f, y - size.height / 2.0f, x + size.width / 2.0f, y - size.height / 2.0f, 0.0f, 8,
              color);
    drawLine_(x + size.width / 2.0f, y - size.height / 2.0f, x + size.width / 2.0f, y + size.height / 2.0f, 0.0f, 8,
              color);
    drawLine_(x + size.width / 2.0f, y + size.height / 2.0f, x - size.width / 2.0f, y + size.height / 2.0f, 0.0f, 8,
              color);
    drawLine_(x - size.width / 2.0f, y + size.height / 2.0f, x - size.width / 2.0f, y - size.height / 2.0f, 0.0f, 8,
              color);
} // 0x803B62A0

/*
 * is_visible
 *
 * A pane is visible when it and all its ancestors are visible.
 */
static bool is_visible(nw4hbm::lyt::Pane* pPane) {
    if (!pPane->IsVisible()) {
        return false;
    }

    if (!pPane->GetParent()) {
        return true;
    }

    // R36: the reference's conditional instead of the plain call; the bool-returning ternary is the
    // spelling retail's codegen comes from.
    return is_visible(pPane->GetParent()) ? true : false;
} // 0x803B6430

/*
 * PaneComponent::isVisible
 *
 * The pane's own visibility, inherited from its parents.
 */
bool PaneComponent::isVisible() {
    return is_visible(mpPane);
} // 0x803B6550

} // namespace gui
} // namespace homebutton
