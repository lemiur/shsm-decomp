#ifndef CLIMAX_COMPANIM_H
#define CLIMAX_COMPANIM_H

// Names and layout: Gormiti link map and DWARF (ClimaxT1Effects/CompAnim.cpp).

#include <revolution/types.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plcore/bavector.h>
#include <rwsdk/tool/anim/rtanim.h>

/*
====================
StCompAnim_Std_F_KeyFrame

A compressed keyframe of the standard HAnim scheme with a float-range rotation and translation: the
rotation in 48 bits, the translation as 16-bit fractions of StCompAnimData_std_f's range.
====================
*/
struct StCompAnim_Std_F_KeyFrame {
    StCompAnim_Std_F_KeyFrame*  prevFrame;  // 0x00
    float                       time;       // 0x04
    unsigned int                Quart_P_32; // 0x08
    unsigned short              Quart_P_16; // 0x0C
    unsigned short              Trans_X;    // 0x0E
    unsigned short              Trans_Y;    // 0x10
    unsigned short              Trans_Z;    // 0x12
};

/*
====================
StCompAnim_Std_P_KeyFrame

A compressed keyframe of the standard HAnim scheme: the rotation as 16-bit components, the translation in
full precision.
====================
*/
struct StCompAnim_Std_P_KeyFrame {
    StCompAnim_Std_P_KeyFrame*  prevFrame;  // 0x00
    float                       time;       // 0x04
    short                       Quart_X;    // 0x08
    short                       Quart_Y;    // 0x0A
    short                       Quart_Z;    // 0x0C
    short                       Quart_W;    // 0x0E
    float                       Trans_X;    // 0x10
    float                       Trans_Y;    // 0x14
    float                       Trans_Z;    // 0x18
};

/*
====================
StCompAnim_Full_F_KeyFrame

A compressed keyframe of the full HAnim scheme: the rotation as 16-bit components, the translation and
the scale as 16-bit fractions of StCompAnimData_full_f's ranges.
====================
*/
struct StCompAnim_Full_F_KeyFrame {
    StCompAnim_Full_F_KeyFrame* prevFrame;  // 0x00
    float                       time;       // 0x04
    short                       Quart_I_X;  // 0x08
    short                       Quart_I_Y;  // 0x0A
    short                       Quart_I_Z;  // 0x0C
    short                       Quart_R;    // 0x0E
    unsigned short              Trans_X;    // 0x10
    unsigned short              Trans_Y;    // 0x12
    unsigned short              Trans_Z;    // 0x14
    unsigned short              Scale_X;    // 0x16
    unsigned short              Scale_Y;    // 0x18
    unsigned short              Scale_Z;    // 0x1A
};

/*
====================
StCompAnim_Std_V2_KeyFrame

A keyframe of the standard HAnim scheme in its variable-length form: the first keyframe of each node is
stored whole, with the translation in full precision. The data that follows (see KeyFrameInterpolate_V2_Std)
is packed as 16-bit words.
====================
*/
struct StCompAnim_Std_V2_KeyFrame {
    union {
        StCompAnim_Std_V2_KeyFrame* prevFrame;
        unsigned int                KeyData;
    } PrevData;                             // 0x00
    float                       time;       // 0x04
    float                       x;          // 0x08
    float                       y;          // 0x0C
    float                       z;          // 0x10
};

/*
====================
StCompAnimData_std_f

The animation-wide data of the standard float scheme (its custom data): the translation of a keyframe is
Tbase plus its 16-bit fraction of TScale.
====================
*/
struct StCompAnimData_std_f {
    RwV3d   Tbase;  // 0x00
    RwV3d   TScale; // 0x0C
};

/*
====================
StCompAnimData_full_f

The animation-wide data of the full float scheme (its custom data): the translation and the scale of a
keyframe are a base plus its 16-bit fraction of a range.
====================
*/
struct StCompAnimData_full_f {
    RwV3d   Tbase;  // 0x00
    RwV3d   TScale; // 0x0C
    RwV3d   Sbase;  // 0x18
    RwV3d   SScale; // 0x24
};

int CompressedAnimRegister();

#endif
