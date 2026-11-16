#ifndef CLIMAX_TEVCBMGR_H
#define CLIMAX_TEVCBMGR_H

// Names and layout: Gormiti link map and DWARF (TevCBMgr.obj).

#include <rwsdk/world/climaxplatform.h>

namespace RenderState {

/*
====================
cTevCBHandle

A registration of render callbacks (the transform, pre-TEV and post-TEV callbacks of the GX pipe).
Every handle lives in a list ordered by falling priority; the callbacks installed in the render
pipe are the merge of the callbacks of all handles (see RebuildCallBacks).
====================
*/
class cTevCBHandle {
public:
    float                           m_Priority;     // 0x00, the handle's place in the list
    RwClimaxRVLTransformCallBack    m_XformCB;      // 0x04
    RwClimaxRVLTevCallBack          m_PreTevCB;     // 0x08
    RwClimaxRVLTevCallBack          m_PostTevCB;    // 0x0C

    cTevCBHandle( float priority );
    ~cTevCBHandle();

    void SetCallBacks( RwClimaxRVLTransformCallBack XformCB, RwClimaxRVLTevCallBack PreTevCB,
                       RwClimaxRVLTevCallBack PostTevCB );
    void RebuildCallBacks();

private:
    static unsigned int Internal_XForm_CB( RwMatrix* ltm, int normals, float ( *ModelView )[4] );
    static unsigned int Internal_PreTEV_CB( StTevSetupData* TevSetupData );
    static unsigned int Internal_PostTEV_CB( StTevSetupData* TevSetupData );

    static cTevCBHandle*    sHandleList[10];
    static int              sHandleCount;
};

}

#endif
