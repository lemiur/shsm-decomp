// Unit name and boundary: Gormiti link map (RpDMorph.a rpdmorph.obj)
// Names, locals and statement order: Gormiti link map and DWARF (RpDMorph.a rpdmorph.obj).
/*
 * rpdmorph.c: the RpDMorph plugin, the delta-morph plugin. It keeps no per-engine data: the engine
 * open and close callbacks count the opens in rpDMorphModule.numInstances, and the attach registers
 * the plugin with the engine, then its geometry and atomic extensions.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>

/* The delta-morph module registration (declared in the header): numInstances is the engine open count. */
RwModuleInfo rpDMorphModule;

/*
 * ==================== DMorphOpen ====================
 * Engine open callback: counts one more open. The engine instance is returned unchanged.
 */
static void* DMorphOpen(void* instance, RwInt32 offset, RwInt32 size) {
    rpDMorphModule.numInstances++;

    return instance;
} // 0x802BABFC

/*
 * ==================== DMorphClose ====================
 * Engine close callback: takes one open away from the count. The engine instance is returned unchanged.
 */
static void* DMorphClose(void* instance, RwInt32 offset, RwInt32 size) {
    rpDMorphModule.numInstances--;

    return instance;
} // 0x802BAC10

/*
 * ==================== RpDMorphPluginAttach ====================
 * Registers the plugin with the engine, then the geometry and atomic extensions. Returns FALSE when
 * the engine has no room for the plugin or one of the extensions fails.
 */
RwBool RpDMorphPluginAttach(void) {
    RwInt32 offset;

    offset = RwEngineRegisterPlugin(0, rwID_DMORPHPLUGIN, DMorphOpen, DMorphClose);
    if (offset < 0) {
        return FALSE;
    }

    if (_rpDMorphGeometryPluginAttach() == FALSE) {
        return FALSE;
    }

    return _rpDMorphAtomicPluginAttach() != FALSE;
} // 0x802BAC24
