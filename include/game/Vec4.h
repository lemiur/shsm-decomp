#ifndef GAME_VEC4_H
#define GAME_VEC4_H

/*
 * Four floats: a position written with a fourth word, the 16-byte vector that the position
 * writer fn_8009ECB0 fills (x, y, z, then a zero w). Header and name invented; no evidence names
 * the fourth word.
 */
struct unk_Vec4 {
    float       x;
    float       y;
    float       z;
    float       w;
};

#endif
