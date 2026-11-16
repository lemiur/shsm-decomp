// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMAnmController.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/HBMAnmController.cpp
#include "HBMAnmController.h"

#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/group.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/LinkList.h"

namespace homebutton {

/*
 * GroupAnmController::GroupAnmController
 *
 * Starts with no group and no animation.
 */
GroupAnmController::GroupAnmController() : mpGroup(NULL), mpAnimGroup(NULL) {} // 0x803B4DC0

/*
 * GroupAnmController::~GroupAnmController
 */
GroupAnmController::~GroupAnmController() {} // 0x803B4DE0

/*
 * GroupAnmController::do_calc
 *
 * Steps the frame while playing and sets the animation's frame, then enables
 * the animation on every pane of the group while playing and disables it
 * otherwise.
 */
void GroupAnmController::do_calc() {
    bool enable;

    if (mState == eState_Playing) {
        calc();
        enable = true;

        mpAnimGroup->SetFrame(mCurFrame);
    } else {
        enable = false;
    }

    nw4hbm::lyt::detail::PaneLink::LinkList& list = mpGroup->GetPaneList();

    NW4HBM_RANGE_FOR(it, list) {
        it->mTarget->SetAnimationEnable(mpAnimGroup, enable, false);
    }
} // 0x803B4E20

} // namespace homebutton
