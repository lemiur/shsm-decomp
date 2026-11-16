#ifndef HBM_ANM_CONTROLLER_H
#define HBM_ANM_CONTROLLER_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMAnmController.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/HBMAnmController.h

#include <revolution/types.h>
#include "HBMFrameController.h"

namespace nw4hbm {
namespace lyt {
class AnimTransform;
class Group;
} // namespace lyt
} // namespace nw4hbm

namespace homebutton {

/*
 * GroupAnmController
 *
 * Plays one layout animation on every pane of a group: do_calc() advances the
 * frame and enables or disables the animation on the group's panes.
 */
class GroupAnmController : public FrameController {
public:
    GroupAnmController();
    virtual ~GroupAnmController();

    void do_calc();

public:
    nw4hbm::lyt::Group* mpGroup;             // 0x20
    nw4hbm::lyt::AnimTransform* mpAnimGroup; // 0x24
}; // size 0x28

} // namespace homebutton

#endif
