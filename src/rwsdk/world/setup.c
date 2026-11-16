// Unit name and boundary: Gormiti link map (RPworld.a setup.obj; its source there is pipe/p2/gcn/setup.c)
// Names, parameters and locals: Gormiti DWARF (Final and Debug builds)
/*
 * setup.c: the GameCube pipeline's lighting set-up for rendering. Each material function loads
 * the GX hardware with the colours of one lighting case: the ambient light, the material, or the
 * two combined, into the TEV registers or into the channel.
 */
#include <revolution/gx/GXLighting.h>
#include <revolution/gx/GXTev.h>
#include <rwsdk/plcore/bacolor.h>
#include <rwsdk/world/climaxcommon.h>

/*
 * Material functions: each loads the GX colours for one lighting case from the ambient light and the
 * material of a mesh (all take the same arguments, so that one function pointer type fits them;
 * the unused ones are named as in Gormiti). They are static in Gormiti; retail exports them because
 * _rwDlObjectRenderSetup, which is not in this unit, selects among them. An RwRGBA material colour is
 * passed to GX as the GXColor of the same four bytes.
 */

void MatFuncDummy() {
} // 0x802DE16C

/* Material colour to TEV register 0, ambient colour times material colour to TEV register 1. */
void MatFunc1(RwRGBAReal* ambientColor, RwRGBA* materialColor, float ambientCoef) {
    GXColor ambMatColor;

    if (RwClimaxPseudoHDR) {
        ambientCoef *= 0.5f;
    }
    ambMatColor.r = (unsigned char)(ambientColor->red * ambientCoef * materialColor->red);
    ambMatColor.g = (unsigned char)(ambientColor->green * ambientCoef * materialColor->green);
    ambMatColor.b = (unsigned char)(ambientColor->blue * ambientCoef * materialColor->blue);
    ambMatColor.a = (unsigned char)(ambientColor->alpha * ambientCoef * materialColor->alpha);
    GXSetTevColor(GX_TEVREG0, *(GXColor*)materialColor);
    GXSetTevColor(GX_TEVREG1, ambMatColor);
} // 0x802DE170

/* Ambient colour to TEV register 1. */
void MatFunc2(RwRGBAReal* ambientColor, float ambientCoef) {
    GXColor ambColor;
    float ambCoef;

    if (RwClimaxPseudoHDR) {
        ambientCoef *= 0.5f;
    }
    ambCoef = 255.0f * ambientCoef;
    ambColor.r = (unsigned char)(ambientColor->red * ambCoef);
    ambColor.g = (unsigned char)(ambientColor->green * ambCoef);
    ambColor.b = (unsigned char)(ambientColor->blue * ambCoef);
    ambColor.a = 0;
    GXSetTevColor(GX_TEVREG1, ambColor);
} // 0x802DE288

/* Ambient colour to the colour channel; the material colour to the alpha channel when it is translucent. */
void MatFunc3(RwRGBAReal* ambientColor, RwRGBA* materialColor, float ambientCoef) {
    GXColor ambColor;
    float ambCoef;

    if (RwClimaxPseudoHDR) {
        ambientCoef *= 0.5f;
    }
    ambCoef = 255.0f * ambientCoef;
    ambColor.r = (unsigned char)(ambientColor->red * ambCoef);
    ambColor.g = (unsigned char)(ambientColor->green * ambCoef);
    ambColor.b = (unsigned char)(ambientColor->blue * ambCoef);
    ambColor.a = 0;
    GXSetChanAmbColor(GX_COLOR0, ambColor);
    if (materialColor->alpha < 255) {
        GXSetChanMatColor(GX_ALPHA0, *(GXColor*)materialColor);
    }
} // 0x802DE324

/* Material colour to the channel, ambient colour to its ambient register. */
void MatFunc4(RwRGBAReal* ambientColor, RwRGBA* materialColor, float ambientCoef) {
    GXColor ambColor;
    float ambCoef;

    if (RwClimaxPseudoHDR) {
        ambientCoef *= 0.5f;
    }
    ambCoef = 255.0f * ambientCoef;
    ambColor.r = (unsigned char)(ambientColor->red * ambCoef);
    ambColor.g = (unsigned char)(ambientColor->green * ambCoef);
    ambColor.b = (unsigned char)(ambientColor->blue * ambCoef);
    ambColor.a = 0;
    GXSetChanMatColor(GX_COLOR0A0, *(GXColor*)materialColor);
    GXSetChanAmbColor(GX_COLOR0, ambColor);
} // 0x802DE3EC

/* Material colour to the channel. */
void MatFunc5(RwRGBAReal* ambientColor, RwRGBA* materialColor, float ambientCoef) {
    GXSetChanMatColor(GX_COLOR0A0, *(GXColor*)materialColor);
} // 0x802DE49C

/* Ambient colour to the channel's material colour. */
void MatFunc6(RwRGBAReal* ambientColor, RwRGBA* materialColor, float ambientCoef) {
    GXColor ambColor;
    float ambCoef;

    if (RwClimaxPseudoHDR) {
        ambientCoef *= 0.5f;
    }
    ambCoef = 255.0f * ambientCoef;
    ambColor.r = (unsigned char)(ambientColor->red * ambCoef);
    ambColor.g = (unsigned char)(ambientColor->green * ambCoef);
    ambColor.b = (unsigned char)(ambientColor->blue * ambCoef);
    ambColor.a = 0;
    GXSetChanMatColor(GX_COLOR0, ambColor);
} // 0x802DE4CC
