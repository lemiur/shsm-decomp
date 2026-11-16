// Unit name and boundary: Gormiti link map (Time.obj).
// Names and layout: Gormiti link map and DWARF (SpiritFrameWork/Time/Time.cpp).
//
// The engine's clock: reads the console's time base and converts it to milliseconds or seconds.

#include <climax/Time.h>
#include <revolution/os/OSTime.h>

namespace RWS {
namespace Time {

/*
====================
RWS::Time::Open

Retail's body is empty.
====================
*/
// 0x802020D0
void Open() {
}

/*
====================
RWS::Time::GetTime

The time base in milliseconds, truncated to 32 bits.
====================
*/
// 0x802020D4
unsigned int GetTime() {
    return OSTicksToMilliseconds( OSGetTime() );
}

/*
====================
RWS::Time::GetSysTime
====================
*/
// 0x8020211C
long long GetSysTime() {
    return OSGetTime();
}

/*
====================
RWS::Time::DifSysTimes
====================
*/
// 0x80202120
long long DifSysTimes( long long a, long long b ) {
    return a - b;
}

/*
====================
RWS::Time::SysTimeToSeconds
====================
*/
// 0x8020212C
float SysTimeToSeconds( long long a ) {
    return (float)a * ( 1.0f / (float)OS_TIMER_CLOCK );
}

}
}
