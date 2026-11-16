// Unit name and boundary: Gormiti link map (RpMatFx.a multiTexGcn.obj)
// Names, locals and statement order: Gormiti DWARF (RpMatFx.a multiTexGcn.obj), Final and Debug builds.
/*
 * multiTexGcn.c: the GameCube side of RpMatFX's multitexture plugin. It registers the plugin and
 * its GameCube data and pipeline plugins in turn, stopping at the first that fails.
 */
#include <rwsdk/plugin/matfx/multitex.h>

/* The multitexture plugin's ID and the size of its extension of a material. */
#define MULTITEXTURE_PLUGIN_ID 0x129
#define MULTITEXTURE_EXTENSION_SIZE 12

/*
 * ==================== _rpMultiTexturePlatformPluginsAttach ====================
 * Attaches the multitexture plugin and the GameCube's data and pipeline plugins. It returns FALSE
 * as soon as one of them fails.
 */
RwBool _rpMultiTexturePlatformPluginsAttach(void) {
    RwBool result;

    result = _rpMultiTexturePluginAttach();
    if (!result) {
        return FALSE;
    }

    result = _rpMaterialRegisterMultiTexturePlugin(rwID_GAMECUBE, MULTITEXTURE_PLUGIN_ID,
                                                   MULTITEXTURE_EXTENSION_SIZE);
    if (!result) {
        return FALSE;
    }

    result = _rpGameCubeMTDataPluginAttach();
    if (!result) {
        return FALSE;
    }

    result = _rpGameCubeMTPipePluginAttach();
    return result;
} // 0x802C1070
