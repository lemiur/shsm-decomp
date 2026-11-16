#ifndef GAME_VEC3_H
#define GAME_VEC3_H

/*
 * Three floats with an inline constructor; the game's own position and direction type
 * (RenderWare's vectors are the plain unk_RwV3d, climax/Frame.h). Header and name invented.
 * The copy constructor is the type's own: CDrawingTest::fn_80178A60 copies vectors float by
 * float, into a temporary for each by-value argument (the implicit copy moves words).
 */
struct unk_Vec3 {
                unk_Vec3() {}
                unk_Vec3( float x, float y, float z ) : x( x ), y( y ), z( z ) {}
                unk_Vec3( const unk_Vec3& v ) : x( v.x ), y( v.y ), z( v.z ) {}

    float       x;
    float       y;
    float       z;
};

#endif
