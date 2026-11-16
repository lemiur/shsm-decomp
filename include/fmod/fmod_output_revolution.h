/*
 * FMOD::OutputRevolution: the Wii (Revolution) output (fmod_output_revolution.cpp). Only the members
 * used so far are declared; it derives from OutputPolled and its size is 0x3B4 (Gormiti DWARF,
 * sibling.exe types OutputRevolution).
 */
#ifndef _FMOD_OUTPUT_REVOLUTION_H
#define _FMOD_OUTPUT_REVOLUTION_H

namespace FMOD {

class FMOD_OUTPUT_DESCRIPTION_EX;

extern unsigned int gRevolutionMainThreadID;   // the id of the thread that runs the main loop (Gormiti DWARF)

class OutputRevolution {
public:
    static FMOD_OUTPUT_DESCRIPTION_EX *getDescriptionEx();

    static unsigned int mDVDReadPriority;   // the priority of the DVD reads (Gormiti DWARF)
};

} // namespace FMOD

#endif /* _FMOD_OUTPUT_REVOLUTION_H */
