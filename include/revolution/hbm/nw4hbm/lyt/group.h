#ifndef NW4HBM_LYT_GROUP_H
#define NW4HBM_LYT_GROUP_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/group.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_group.h

#include <revolution/types.h>
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/LinkList.h"

namespace nw4hbm {
namespace lyt {

namespace res {

static const u32 SIGNATURE_GROUP_BLOCK = 'grp1';
static const u32 SIGNATURE_GROUP_BLOCK_START = 'grs1';
static const u32 SIGNATURE_GROUP_BLOCK_END = 'gre1';

/*
 * res::Group
 *
 * Layout-file data block of a group ("grp1"); the names of its panes follow the header, 16
 * characters each.
 */
struct Group {
    static const u32 NAME_LENGTH = 16;

    DataBlockHeader blockHeader; // 0x00
    char name[NAME_LENGTH];      // 0x08
    u16 paneNum;                 // 0x18
    u8 padding[2];               // 0x1A // pad: SDK res::Group.padding
}; // size 0x1C

} // namespace res

/* A group's pane list, as the menu's animation setup names it (the reference's PaneLinkList). */
typedef ut::LinkList<detail::PaneLink, 0> PaneLinkList;

/*
 * Group
 *
 * A named set of panes that an animation is applied to together.
 */
class Group {
public:
    typedef ut::LinkList<Group, 4> LinkList;

    Group(); // no copy in retail
    Group(const res::Group* pResGroup, Pane* pRootPane);
    virtual ~Group();

    detail::PaneLink::LinkList& GetPaneList() { return mPaneLinkList; }
    const char* GetName() const { return mName; }
    bool IsUserAllocated() const { return mbUserAllocated; }

    void Init();
    void AppendPane(Pane* pPane);

    static const u32 NAME_LENGTH = res::Group::NAME_LENGTH;

private:
    ut::LinkListNode mLink;                   // 0x04
    detail::PaneLink::LinkList mPaneLinkList; // 0x0C
    char mName[NAME_LENGTH];                  // 0x18
    bool mbUserAllocated;                     // 0x28
}; // size 0x2C

/*
 * GroupContainer
 *
 * The groups of a layout.
 */
class GroupContainer {
public:
    GroupContainer() {}
    ~GroupContainer();

    void AppendGroup(Group* pGroup);
    Group* FindGroupByName(const char* findName);

private:
    Group::LinkList mGroupList; // 0x00
}; // size 0x0C

} // namespace lyt
} // namespace nw4hbm

#endif
