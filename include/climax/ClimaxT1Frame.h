// Names and layout: Gormiti link map and DWARF (ClimaxT1Frame.obj)
#ifndef CLIMAX_CLIMAXT1FRAME_H
#define CLIMAX_CLIMAXT1FRAME_H

#include <rwsdk/baframe.h>
#include <rwsdk/plcore/bamatrix.h>
#include <rwsdk/plcore/bavector.h>

/*
====================
ClimaxT1Frame

The ClimaxT1 plugin data of a frame: how the frame faces the camera, its position and its scale. The
plugin's data sits LocalOffset bytes into the frame.
====================
*/
class ClimaxT1Frame {
public:
    int mode;    // 0x00: how the frame faces the camera, 0 (not at all) to 3
    RwV3d pos;   // 0x04
    float scale; // 0x10
};

/*
====================
ClimaxT1FrameUpdateCBData

What ClimaxT1FrameUpdate hands to the callback that updates the children of a frame: the camera's
orientation and the camera's position.
====================
*/
class ClimaxT1FrameUpdateCBData {
public:
    RwMatrix tmatrix; // 0x00
    RwV3d tpos;       // 0x40
};

extern "C" {
int ClimaxT1FramePluginAttach();
void ClimaxT1FrameUpdate( RwFrame* pFrame, RwFrame* pCamera );
int ClimaxT1FrameHasFacingFrames( RwFrame* pFrame );
}

#endif
