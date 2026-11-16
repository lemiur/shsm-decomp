#ifndef HBM_GUI_MANAGER_H
#define HBM_GUI_MANAGER_H

// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/revolution/hbm/homebutton/HBMGUIManager.hpp
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMGUIManager.h

#include <revolution/types.h>
#include <revolution/kpad.h>
#include <revolution/mem/allocator.h>
#include <revolution/mtx.h>

#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/list.h"

namespace nw4hbm {
namespace lyt {
class DrawInfo;
class Layout;
} // namespace lyt
} // namespace nw4hbm

namespace homebutton {
namespace gui {

class Manager;
class PaneComponent;

/*
 * Interface
 *
 * The menu's update interface: the managers and components the HBM front end
 * drives through one pointer (create, init, calc, draw).
 */
class Interface {
public:
    Interface() {}

    /* 0x08 */ virtual void create() {}
    /* 0x0C */ virtual void init() {}
    /* 0x10 */ virtual void calc() {}
    /* 0x14 */ virtual void draw(Mtx&) {}
    /* 0x18 */ virtual void draw() {}
    /* 0x1C */ virtual ~Interface() {}

private:
    /* 0x00 (vtable) */
}; // size = 0x04

/*
 * EventHandler
 *
 * Receives a component's events (point, move, trigger) with the component's ID.
 */
class EventHandler {
public:
    EventHandler() {}

    /* 0x08 */ virtual void onEvent(u32, u32, void*) {}
    /* 0x0C */ virtual void setManager(Manager* pManager) { mpManager = pManager; }

protected:
    /* 0x00 (vtable) */
    /* 0x04 */ Manager* mpManager;
}; // size = 0x08

/*
 * Component
 *
 * One thing the menu can point at: a pane (PaneComponent) or a whole layout
 * scene. The manager routes the pointer state to it and it reports its events
 * back through the event handler.
 */
class Component : public Interface {
public:
    Component(u32 uID) :
        mDragStartPos(),
        mbDragging(),
        muDraggingButton(),
        muID(uID),
        mbTriggerTarger(),
        mpManager() {
        init();
    }

    /* 0x0C */ virtual void init() {
        mbDragging = false;

        for (int i = 0; i < (int)sizeof(mabPointed); i++) {
            mabPointed[i] = false;
        }
    }

    /* 0x1C */ virtual ~Component() {}
    /* 0x20 */ virtual u32 getID() { return muID; }
    /* 0x24 */ virtual int isPointed(int n) { return mabPointed[n]; }
    /* 0x28 */ virtual void setPointed(int n, bool b) { mabPointed[n] = b; }
    /* 0x2C */ virtual void onPoint() {}
    /* 0x30 */ virtual void offPoint() {}
    /* 0x34 */ virtual void onDrag(f32, f32) {}
    /* 0x38 */ virtual void onMove(f32, f32) {}

    /* 0x3C */ virtual void onTrig(u32 uFlag, Vec& vec) {
        if (uFlag & muDraggingButton) {
            mDragStartPos = vec;
            mbDragging = true;
        }
    }

    /* 0x40 */ virtual void setDraggingButton(u32 uDraggingButton) { muDraggingButton = uDraggingButton; }
    /* 0x44 */ virtual bool update(int, const KPADStatus*, f32, f32, void*) { return false; }
    /* 0x48 */ virtual bool update(int i, f32 x, f32 y, u32 uTrigFlag, u32 uHoldFlag, u32 uReleaseFlag, void* pData);
    /* 0x4C */ virtual bool isTriggerTarger() { return mbTriggerTarger; }
    /* 0x50 */ virtual void setTriggerTarget(bool bTriggerTarget) { mbTriggerTarger = bTriggerTarget; }
    /* 0x54 */ virtual void setManager(Manager* pManager) { mpManager = pManager; }
    /* 0x58 */ virtual bool isVisible() { return true; }
    /* 0x5C */ virtual bool contain(f32 x_, f32 y_) = 0;

protected:
    /* 0x00 (base) */
    /* 0x04 */ bool mabPointed[8];
    /* 0x0C */ Vec mDragStartPos;
    /* 0x18 */ bool mbDragging;
    /* 0x1C */ u32 muDraggingButton;
    /* 0x20 */ u32 muID;
    /* 0x24 */ bool mbTriggerTarger;
    /* 0x28 */ Manager* mpManager;
}; // size = 0x2C

/*
 * Manager
 *
 * Owns the components of one layout scene, keyed by ID, and forwards the
 * pointer state to each of them.
 */
class Manager : public Interface {
    // nested types
private:
    struct IDToComponent {
    public:
        /* 0x00 */ u32 muID;
        /* 0x04 */ Component* mpComponent;
        /* 0x08 */ nw4hbm::ut::Link mLink;

        IDToComponent(u32 uID, Component* pComponent) :
            muID(uID),
            mpComponent(pComponent) {}
    }; // size = 0x10

public:
    Manager(EventHandler* pEventHandler, MEMAllocator* pAllocator) :
        mpEventHandler(pEventHandler),
        mpAllocator(pAllocator) {
        if (mpEventHandler) {
            mpEventHandler->setManager(this);
        }

        nw4hbm::ut::List_Init(&mIDToComponent, 8);
    }

    /* 0x0C */ virtual void init();
    /* 0x10 */ virtual void calc();
    /* 0x18 */ virtual void draw();
    /* 0x1C */ virtual ~Manager();
    /* 0x20 */ virtual void addComponent(Component* pComponent);
    /* 0x24 */ virtual void delComponent(Component* pComponent);
    /* 0x28 */ virtual Component* getComponent(u32 uID);
    /* 0x2C */ virtual bool update(int, const KPADStatus*, f32, f32, void*) { return false; }
    /* 0x30 */ virtual bool update(int i, f32 x, f32 y, u32 uTrigFlag, u32 uHoldFlag, u32 uReleaseFlag, void* pData);

    /* 0x34 */ virtual void onEvent(u32 uID, u32 uEvent, void* pData) {
        if (mpEventHandler) {
            mpEventHandler->onEvent(uID, uEvent, pData);
        }
    }

    /* 0x38 */ virtual void setAllComponentTriggerTarget(bool b);

    /* 0x3C */ virtual void setEventHandler(EventHandler* pEventHandler) {
        mpEventHandler = pEventHandler;

        if (mpEventHandler) {
            mpEventHandler->setManager(this);
        }
    }

protected:
    /* 0x00 (base) */
    /* 0x04 */ EventHandler* mpEventHandler;
    /* 0x08 */ nw4hbm::ut::List mIDToComponent;
    /* 0x14 */ MEMAllocator* mpAllocator;
}; // size = 0x18

/*
 * PaneManager
 *
 * The manager of one layout scene: it walks the pane tree and gives every pane
 * a PaneComponent, so the menu can be driven pane by pane.
 */
class PaneManager : public Manager {
    // nested types
private:
    struct PaneToComponent {
    public:
        PaneToComponent(nw4hbm::lyt::Pane* pPane, PaneComponent* pComponent) :
            mpPane(pPane),
            mpComponent(pComponent) {}

    public:
        /* 0x00 */ nw4hbm::lyt::Pane* mpPane;
        /* 0x04 */ PaneComponent* mpComponent;
        /* 0x08 */ nw4hbm::ut::Link mLink;
    }; // size = 0x10

public:
    PaneManager(EventHandler* pEventHandler, const nw4hbm::lyt::DrawInfo* pDrawInfo, MEMAllocator* pAllocator) :
        Manager(pEventHandler, pAllocator),
        mpDrawInfo(pDrawInfo) {
        nw4hbm::ut::List_Init(&mPaneToComponent, 8);
    }

    /* 0x1C */ virtual ~PaneManager();
    /* 0x40 */ virtual void createLayoutScene(const nw4hbm::lyt::Layout& rLayout);
    /* 0x44 */ virtual void addLayoutScene(const nw4hbm::lyt::Layout& rLayout);
    /* 0x48 */ virtual void delLayoutScene(const nw4hbm::lyt::Layout& rLayout);
    /* 0x4C */ virtual PaneComponent* getPaneComponentByPane(nw4hbm::lyt::Pane* pPane);
    /* 0x50 */ virtual const nw4hbm::lyt::DrawInfo* getDrawInfo() { return mpDrawInfo; }
    /* 0x54 */ virtual void setDrawInfo(const nw4hbm::lyt::DrawInfo* pDrawInfo) { mpDrawInfo = pDrawInfo; }
    /* 0x58 */ virtual void setAllBoundingBoxComponentTriggerTarget(bool b);
    /* 0x5C */ virtual void walkInChildren(nw4hbm::lyt::PaneList& rPaneList);
    /* 0x60 */ virtual void walkInChildrenDel(nw4hbm::lyt::PaneList& rPaneList);

private:
    /* 0x00 (base) */
    /* 0x18 */ nw4hbm::ut::List mPaneToComponent;
    /* 0x24 */ const nw4hbm::lyt::DrawInfo* mpDrawInfo;
    /* 0x28 */ u16 muNumPoint;
    /* 0x2A */ u16 muPadding; // pad: SDK PaneManager.padding (the reference's byte2_t, alignment)

    // static members
    static u32 suIDCounter;
}; // size = 0x2C

/*
 * PaneComponent
 *
 * The menu's handle on one pane: whether the pointer is inside it and the
 * debug outline the menu draws around it.
 */
class PaneComponent : public Component {
public:
    PaneComponent(u32 uID) :
        Component(uID),
        mpPane() {}

    /* 0x18 */ virtual void draw();
    /* 0x1C */ virtual ~PaneComponent() {}
    /* 0x58 */ virtual bool isVisible();
    /* 0x5C */ virtual bool contain(f32 x_, f32 y_);
    /* 0x60 */ virtual void setPane(nw4hbm::lyt::Pane* pPane) { mpPane = pPane; }
    /* 0x64 */ virtual nw4hbm::lyt::Pane* getPane() { return mpPane; }

protected:
    /* 0x00 (base) */
    /* 0x2C */ nw4hbm::lyt::Pane* mpPane;
}; // size = 0x30

} // namespace gui
} // namespace homebutton

#endif
