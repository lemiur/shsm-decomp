/*
 * FMOD::ChannelRealManual3D: the real channel that is positioned by the application in 3D space.
 * Only the member the Revolution channel's base layout needs is named. Layout: Gormiti DWARF
 * (sibling.exe types ChannelRealManual3D), size 0x68.
 */
#ifndef _FMOD_CHANNEL_REALMANUAL3D_H
#define _FMOD_CHANNEL_REALMANUAL3D_H

#include "fmod_channel_real.h"

namespace FMOD {

class ChannelRealManual3D : public ChannelReal {
public:
    float mAngleToListener;                 // 0x64
};

} // namespace FMOD

#endif /* _FMOD_CHANNEL_REALMANUAL3D_H */
