#ifndef FMOD_FMODWII_H
#define FMOD_FMODWII_H

// FMOD Ex 4.x for the Wii (the public fmodwii.h). Layout: Gormiti DWARF (sibling.exe types FMOD_WII_INFO).

/*
 * The extra driver data of System::init on the Wii (and EventSystem::init, which passes it on).
 */
typedef struct FMOD_WII_INFO {
    int                 dvdreadpriority;            // 0x00
    int                 muteremotespeakerifnosound; // 0x04
    int                 audiolibsinitialized;       // 0x08
} FMOD_WII_INFO;

#endif
