#ifndef HBM_FRAME_CONTROLLER_H
#define HBM_FRAME_CONTROLLER_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/HBMFrameController.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/HBMFrameController.h

#include <revolution/types.h>

namespace homebutton {

/*
 * FrameController
 *
 * Steps an animation frame between mMinFrame and mMaxFrame by mFrameDelta per
 * calc(): forward or backward once, wrapping, or back and forth.
 */
class FrameController {
public:
    enum eState {
        eState_Stopped,
        eState_Playing
    };

    enum eAnmType {
        eAnmType_Forward,
        eAnmType_Backward,
        eAnmType_Wrap,
        eAnmType_Alternate,

        eAnmType_Max
    };

    FrameController() {}
    virtual ~FrameController() {}

    virtual void calc();

    void init(int anmType, f32 maxFrame, f32 minFrame, f32 delta);
    void initFrame();

    f32 getMaxFrame() const { return mMaxFrame; }
    f32 getCurrentFrame() const { return mCurFrame; }
    f32 getLastFrame() const { return mMaxFrame - 1.0f; }
    bool isPlaying() const { return mState == eState_Playing; }

    void setMaxFrame(f32 value) { mMaxFrame = value; }
    void setMinFrame(f32 value) { mMinFrame = value; }
    f32 getMinFrame() const { return mMinFrame; }
    void setCurrentFrame(f32 value) { mCurFrame = value; }
    void setDelta(f32 value) { mFrameDelta = value; }
    f32 getDelta() const { return mFrameDelta; }
    void setState(int value) { mState = value; }
    int getState() const { return mState; }
    void setAnimType(int value) { mAnmType = value; }
    int getAnimType() const { return mAnmType; }

    void start() {
        initFrame();
        restart();
    }
    void stop() { mState = eState_Stopped; }
    void restart() { mState = eState_Playing; }

protected:
    f32 mMaxFrame;   // 0x04
    f32 mMinFrame;   // 0x08
    f32 mCurFrame;   // 0x0C
    f32 mFrameDelta; // 0x10 frames per calc()
    int mState;      // 0x14 eState
    int mAnmType;    // 0x18 eAnmType
    bool mAltFlag;   // 0x1C eAnmType_Alternate: running backward
};

} // namespace homebutton

#endif
