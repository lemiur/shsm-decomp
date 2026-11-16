// Unit name and boundary: Gormiti link map (TevCBMgr.obj).
// Names and layout: Gormiti link map and DWARF (TevCBMgr.obj).
//
// The manager of the render callbacks: the handles registered with it are kept ordered by priority, and
// the callbacks of all of them are merged into the one set the render pipe calls.

#include <climax/TevCBMgr.h>

namespace RenderState {

cTevCBHandle*   cTevCBHandle::sHandleList[10];
int             cTevCBHandle::sHandleCount;

/*
====================
cTevCBHandle::cTevCBHandle

Registers the handle in the list, after the handles of a higher priority.
====================
*/
// 0x801F4FF8
cTevCBHandle::cTevCBHandle( float priority ) {
    m_Priority = priority;

    int pos = 0;
    while ( pos < sHandleCount && priority < sHandleList[pos]->m_Priority ) {
        pos++;
    }

    for ( int tpos = sHandleCount; tpos > pos; tpos-- ) {
        sHandleList[tpos] = sHandleList[tpos - 1];
    }
    sHandleList[pos] = this;
    sHandleCount++;
}

/*
====================
cTevCBHandle::~cTevCBHandle

Takes the handle out of the list and installs the callbacks of those that remain.
====================
*/
// 0x801F51C0
cTevCBHandle::~cTevCBHandle() {
    int pos = 0;
    while ( sHandleList[pos] != this && pos < sHandleCount ) {
        pos++;
    }

    for ( ; pos < sHandleCount; pos++ ) {
        sHandleList[pos] = sHandleList[pos + 1];
    }
    sHandleCount--;

    RebuildCallBacks();
}

/*
====================
cTevCBHandle::SetCallBacks

Sets the handle's callbacks and installs the merged set.
====================
*/
// 0x801F52E0
void cTevCBHandle::SetCallBacks( RwClimaxRVLTransformCallBack XformCB, RwClimaxRVLTevCallBack PreTevCB,
                                 RwClimaxRVLTevCallBack PostTevCB ) {
    m_XformCB = XformCB;
    m_PreTevCB = PreTevCB;
    m_PostTevCB = PostTevCB;
    RebuildCallBacks();
}

/*
====================
cTevCBHandle::RebuildCallBacks

Installs the callbacks of the registered handles: a callback that only one handle sets is installed
as it is, and where several handles set the same kind the manager's own callback, which calls them
all, is installed in its place.
====================
*/
// 0x801F52F0
void cTevCBHandle::RebuildCallBacks() {
    RwClimaxRVLTransformCallBack XformCB = NULL;
    RwClimaxRVLTevCallBack PreTevCB = NULL;
    RwClimaxRVLTevCallBack PostTevCB = NULL;

    for ( int pos = 0; pos < sHandleCount; pos++ ) {
        cTevCBHandle* Handle = sHandleList[pos];
        if ( Handle->m_XformCB ) {
            if ( XformCB ) {
                XformCB = Internal_XForm_CB;
            } else {
                XformCB = Handle->m_XformCB;
            }
        }
        if ( Handle->m_PreTevCB ) {
            if ( PreTevCB ) {
                PreTevCB = Internal_PreTEV_CB;
            } else {
                PreTevCB = Handle->m_PreTevCB;
            }
        }
        if ( Handle->m_PostTevCB ) {
            if ( PostTevCB ) {
                PostTevCB = Internal_PostTEV_CB;
            } else {
                PostTevCB = Handle->m_PostTevCB;
            }
        }
    }
    RwClimaxRVLSetRenderCallbacks( XformCB, PreTevCB, PostTevCB );
}

/*
====================
cTevCBHandle::Internal_XForm_CB

Calls the transform callbacks of the handles, by falling priority.
====================
*/
// 0x801F5390
unsigned int cTevCBHandle::Internal_XForm_CB( RwMatrix* ltm, int normals, float ( *ModelView )[4] ) {
    unsigned int rv = 0;

    for ( int pos = 0; pos < sHandleCount; pos++ ) {
        cTevCBHandle* Handle = sHandleList[pos];
        if ( Handle->m_XformCB ) {
            rv = Handle->m_XformCB( ltm, normals, ModelView );
        }
    }
    return rv;
}

/*
====================
cTevCBHandle::Internal_PreTEV_CB

Calls the pre-TEV callbacks of the handles, by falling priority.
====================
*/
// 0x801F540C
unsigned int cTevCBHandle::Internal_PreTEV_CB( StTevSetupData* TevSetupData ) {
    unsigned int rv = 0;

    for ( int pos = 0; pos < sHandleCount; pos++ ) {
        cTevCBHandle* Handle = sHandleList[pos];
        if ( Handle->m_PreTevCB ) {
            rv = Handle->m_PreTevCB( TevSetupData );
        }
    }
    return rv;
}

/*
====================
cTevCBHandle::Internal_PostTEV_CB

Calls the post-TEV callbacks of the handles, by rising priority (the reverse of the pre-TEV order).
====================
*/
// 0x801F5488
unsigned int cTevCBHandle::Internal_PostTEV_CB( StTevSetupData* TevSetupData ) {
    unsigned int rv = 0;

    for ( int pos = sHandleCount - 1; pos >= 0; pos-- ) {
        cTevCBHandle* Handle = sHandleList[pos];
        if ( Handle->m_PostTevCB ) {
            Handle->m_PostTevCB( TevSetupData );
        }
    }
    return rv;
}

}
