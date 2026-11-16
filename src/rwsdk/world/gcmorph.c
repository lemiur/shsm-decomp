// Unit name and boundary: Gormiti link map (RPworld.a gcmorph)
// Parameters: Gormiti DWARF
/*
 * gcmorph.c: the GameCube morph target interpolation. The only function left in this build is the
 * vector interpolation stub, which does nothing.
 */
#include <rwsdk/world/rpplugin.h>

/* Interpolates between two arrays of vectors; empty in this build. */
void _rwDlV3dInterp(void* dst, void* src1, void* src2, float* scale, int count, unsigned int inSize,
                    unsigned int outSize, int outSkip) {
} // 0x802DC670
