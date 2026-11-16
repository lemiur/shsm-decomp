// Names and layout: Gormiti link map and DWARF (ParticleX RwPlugin.obj)
#ifndef CLIMAX_RWPLUGIN_H
#define CLIMAX_RWPLUGIN_H

#include <rwsdk/tool/dict/rtdict.h>

// Registers the effects' plugins with RenderWare: the atomic plugin and the dictionary of effect entries.
int ClimaxP1EffectPluginAttach();

// The schema of the dictionary of effect entries.
RtDictSchema* ClimaxP1DictionaryGetSchema();

#endif
