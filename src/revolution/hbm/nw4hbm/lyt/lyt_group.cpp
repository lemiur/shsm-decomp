// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_group.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_group.cpp
#include "nw4hbm/lyt/group.h"

#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/ut/LinkList.h"

#include <new>
#include <string.h>

namespace nw4hbm {
namespace lyt {

/*
 * Group::Group
 *
 * Builds the group from its data block, linking each pane the block names that
 * is found under `pRootPane`.
 */
Group::Group(const res::Group* pResGroup, Pane* pRootPane) {
    Init();
    memcpy(mName, pResGroup->name, sizeof(mName));

    const char* paneName = detail::ConvertOffsToPtr<char>(pResGroup, sizeof(*pResGroup));

    for (int i = 0; i < pResGroup->paneNum; ++i) {
        Pane* pFindPane = pRootPane->FindPaneByName(paneName + (int)NAME_LENGTH * i, true);

        if (pFindPane) {
            AppendPane(pFindPane);
        }
    }
} // 0x803C2E80

/*
 * Group::Init
 */
void Group::Init() {
    mbUserAllocated = false;
} // no copy in retail (inlined)

/*
 * Group::~Group
 *
 * Releases the pane links.
 */
Group::~Group() {
    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mPaneLinkList) {
        __typeof__(it) currIt = it++;

        mPaneLinkList.Erase(currIt);
        Layout::FreeMemory(&(*currIt));
    }
} // 0x803C2F90

/*
 * Group::AppendPane
 *
 * Adds a link to `pPane` at the end of the group's pane list.
 */
void Group::AppendPane(Pane* pPane) {
    if (void* pMem = Layout::AllocMemory(sizeof(detail::PaneLink))) {
        detail::PaneLink* pPaneLink = new (pMem) detail::PaneLink;

        pPaneLink->mTarget = pPane;
        mPaneLinkList.PushBack(pPaneLink);
    }
} // no copy in retail (inlined)

/*
 * GroupContainer::~GroupContainer
 *
 * Destroys the groups the layout allocated; user-allocated ones are only unlinked.
 */
GroupContainer::~GroupContainer() {
    NW4HBM_RANGE_FOR_NO_AUTO_INC(it, mGroupList) {
        __typeof__(it) currIt = it++;

        mGroupList.Erase(currIt);

        if (!currIt->IsUserAllocated()) {
            currIt->~Group();
            Layout::FreeMemory(&(*currIt));
        }
    }
} // 0x803C3040

/*
 * GroupContainer::AppendGroup
 */
void GroupContainer::AppendGroup(Group* pGroup) {
    mGroupList.PushBack(pGroup);
} // 0x803C3110

/*
 * GroupContainer::FindGroupByName
 *
 * Returns the group named `findName`, or NULL.
 */
Group* GroupContainer::FindGroupByName(const char* findName) {
    NW4HBM_RANGE_FOR(it, mGroupList) {
        if (detail::EqualsPaneName(it->GetName(), findName)) {
            return &(*it);
        }
    }

    return NULL;
} // 0x803C3150

} // namespace lyt
} // namespace nw4hbm
