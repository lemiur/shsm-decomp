// Unit name and boundary: Gormiti link map (fmod_event.a fmod_event.obj)

#include <stddef.h>
#include <fmod/fmod.h>
#include <fmod/fmod_event.h>

/*
====================
FMOD_EventSystem_Create
====================
*/
extern "C" FMOD_RESULT F_API FMOD_EventSystem_Create( FMOD_EVENTSYSTEM **eventsystem ) {
    return FMOD::EventSystem_CreateInternal( (FMOD::EventSystem **)eventsystem, NULL );
} // 0x80384A4C
