#ifndef CLIMAX_TIME_H
#define CLIMAX_TIME_H

// Names and layout: Gormiti link map and DWARF (SpiritFrameWork/Time/Time.cpp; RWS::Time is a namespace).

#include <revolution/types.h>

namespace RWS {
namespace Time {

// Time.obj: the engine's clock, a thin layer over the SDK's OSTime.
void            Open();
unsigned int    GetTime();                          // milliseconds
long long       GetSysTime();                       // raw system ticks
long long       DifSysTimes( long long a, long long b );
float           SysTimeToSeconds( long long a );

}
}

#endif
