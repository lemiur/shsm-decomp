// Names and layout: Gormiti link map and DWARF (ClimaxT1Dictionary.obj)
#ifndef CLIMAX_CLIMAXT1DICTIONARY_H
#define CLIMAX_CLIMAXT1DICTIONARY_H

#include <rwsdk/tool/anim/rtanim.h>
#include <rwsdk/tool/dict/rtdict.h>

/*
====================
ClimaxT1Entry

An entry of the effects dictionary: an animation found by its index, shared by reference count.
====================
*/
class ClimaxT1Entry {
public:
    char* name;                 // 0x0: the dictionary name of the entry
    int referenceCount;         // 0x4
    int animationIndex;         // 0x8: the index the animation is found by
    RtAnimAnimation* animation; // 0xC
};

extern "C" {
int ClimaxT1DictionaryPluginAttach();
RtDictSchema* ClimaxT1DictionaryGetSchema();
RtAnimAnimation* ClimaxT1DictionarySchemaGetAnimation( int animation );
RtAnimAnimation* ClimaxT1DictionaryGetAnimation( RtDict* cmDict, int animation );
}

#endif
