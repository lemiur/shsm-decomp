// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMFrameController.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/HBMFrameController.cpp
#include "HBMFrameController.h"

namespace homebutton {

/*
 * FrameController::init
 *
 * Sets the animation type and frame range, stopped at the first frame.
 */
void FrameController::init(int anmType, f32 maxFrame, f32 minFrame, f32 delta) {
    mAnmType = anmType;
    mMaxFrame = maxFrame;
    mMinFrame = minFrame;
    mFrameDelta = delta;
    mState = eState_Stopped;
    mAltFlag = false;

    initFrame();
} // 0x803B4C10

/*
 * FrameController::initFrame
 *
 * Rewinds to the first frame: mMaxFrame when playing backward, else mMinFrame.
 */
void FrameController::initFrame() {
    mCurFrame = mAnmType == eAnmType_Backward ? mMaxFrame : mMinFrame;
} // 0x803B4C50

/*
 * FrameController::calc
 *
 * Advances one step while playing. Forward and backward stop at the last
 * frame, wrap starts over, alternate turns around at either end.
 */
void FrameController::calc() {
    if (mState != eState_Playing) {
        return;
    }

    switch (mAnmType) {
    case eAnmType_Forward:
        if ((mCurFrame += mFrameDelta) >= getLastFrame()) {
            mCurFrame = getLastFrame();
            stop();
        }
        break;

    case eAnmType_Backward:
        if ((mCurFrame -= mFrameDelta) <= mMinFrame) {
            mCurFrame = mMinFrame;
            stop();
        }
        break;

    case eAnmType_Wrap:
        if ((mCurFrame += mFrameDelta) >= mMaxFrame) {
            mCurFrame -= mMaxFrame - mMinFrame;
        }
        break;

    case eAnmType_Alternate:
        if (!mAltFlag) {
            if ((mCurFrame += mFrameDelta) >= getLastFrame()) {
                mCurFrame = getLastFrame();
                mAltFlag = true;
            }
        } else {
            if ((mCurFrame -= mFrameDelta) <= mMinFrame) {
                mCurFrame = mMinFrame;
                mAltFlag = false;
            }
        }
        break;
    }
} // 0x803B4C70

} // namespace homebutton
