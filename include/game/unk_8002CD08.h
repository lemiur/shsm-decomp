#ifndef GAME_UNK_8002CD08_H
#define GAME_UNK_8002CD08_H

// Unit name unconfirmed: see src/game/unk_8002CD08.cpp.

#include <game/CPointOfInterest.h>
#include <game/Vec3.h>
#include <game/ZoneLink.h>

extern "C" {
void    fn_8002CD08( ZoneLink* self );
void*   fn_8002CDF8( ZoneLink* self, int index, int flag );
void*   fn_8002CE54( ZoneLink* self, int index, int flag );
void*   fn_8002CEB0( ZoneLink* self, int flag );
void*   fn_8002CEF0( ZoneLink* self, int flag );
void*   fn_8002CF30( ZoneLink* self, int index, int flag );
void*   fn_8002CF7C( ZoneLink* self, int index, int flag, int alt );
void*   fn_8002CFE4( ZoneLink* self, int index, int flag, int alt );
void*   fn_8002D04C( ZoneLink* self, int index, int flag );
bool    fn_8002D0B4( ZoneLink* self );
void    fn_8002D100( unk_Vec3* out, void* owner );
}

#endif
