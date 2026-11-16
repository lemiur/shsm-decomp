#ifndef GAME_VECTORPS_H
#define GAME_VECTORPS_H

// unit name unconfirmed: header name invented. The game's paired-single vector helpers, inline
// asm (C cannot express ps_add/ps_muls0), scheduled by MWCC into the code around them; their
// original header is not identified. CAmbientInterferencePoint and unk_8000A040 keep unit-local
// copies of V3dNormalize, V3dDot and V3dAdd (V3dAdd the same; their V3dNormalize uses one
// register for the ps_madd and ps_sum0 results, where CDrawingTest's retail uses two, and their
// V3dDot names its operands the other way round). Names invented.

#include <climax/Frame.h>

/*
====================
V3dAdd

dst = a + b.
====================
*/
inline void V3dAdd( register unk_RwV3d* dst, register const unk_RwV3d* a, register const unk_RwV3d* b ) {
    asm {
        psq_l       fp2, 0(a), 0, 0
        psq_l       fp3, 8(a), 1, 0
        psq_l       fp1, 8(b), 1, 0
        ps_add      fp3, fp3, fp1
        psq_l       fp0, 0(b), 0, 0
        psq_st      fp3, 8(dst), 1, 0
        ps_add      fp2, fp2, fp0
        psq_st      fp2, 0(dst), 0, 0
    }
}

/*
====================
V3dSub

dst = a - b.
====================
*/
inline void V3dSub( register unk_RwV3d* dst, register const unk_RwV3d* a, register const unk_RwV3d* b ) {
    asm {
        psq_l       fp2, 0(a), 0, 0
        psq_l       fp1, 8(b), 1, 0
        psq_l       fp3, 8(a), 1, 0
        ps_sub      fp5, fp3, fp1
        psq_l       fp0, 0(b), 0, 0
        ps_sub      fp4, fp2, fp0
        psq_st      fp4, 0(dst), 0, 0
        psq_st      fp5, 8(dst), 1, 0
    }
}

/*
====================
V3dLength

The length of v, 0 for a zero vector; paired-single estimate.
====================
*/
inline float V3dLength( register const unk_RwV3d* v ) {
    register float length = 0.0f;
    asm {
        psq_l       fp0, 0(v), 0, 0
        ps_mul      fp0, fp0, fp0
        psq_l       fp1, 8(v), 1, 0
        ps_mul      fp1, fp1, fp1
        ps_sum0     fp2, fp0, fp0, fp0
        fadds       fp1, fp1, fp2
        fcmpu       cr0, fp1, length
        beq         done
        ps_rsqrte   fp2, fp1
        ps_res      length, fp2
    done:
    }
    return length;
}

/*
====================
V3dScale

dst = v * s.
====================
*/
inline void V3dScale( register unk_RwV3d* dst, register const unk_RwV3d* v, register float s ) {
    asm {
        psq_l       fp0, 0(v), 0, 0
        psq_l       fp1, 8(v), 1, 0
        ps_muls0    fp2, fp0, s
        ps_muls0    fp3, fp1, s
        psq_st      fp2, 0(dst), 0, 0
        psq_st      fp3, 8(dst), 1, 0
    }
}

/*
====================
V3dNormalize

Scales v to unit length, leaving a zero vector alone.
====================
*/
inline void V3dNormalize( register unk_RwV3d* v ) {
    register float zero = 0.0f;
    register float half = 0.5f;
    register float three = 3.0f;
    register float xy;
    register float z;
    register float lengthSq;
    register float work1;
    register float work0;
    asm {
        psq_l       xy, 0(v), 0, 0
        psq_l       z, 8(v), 1, 0
        ps_mul      work0, xy, xy
        ps_madd     work1, z, z, work0
        ps_sum0     lengthSq, work1, z, work0
        fcmpu       cr0, lengthSq, zero
        beq         done
        frsqrte     work0, lengthSq
        fmuls       work1, work0, work0
        fmuls       work0, work0, half
        fnmsubs     work1, work1, lengthSq, three
        fmuls       work0, work1, work0
        ps_muls0    xy, xy, work0
        ps_muls0    z, z, work0
        psq_st      xy, 0(v), 0, 0
        psq_st      z, 8(v), 1, 0
    done:
    }
}

/*
====================
V3dDot

a . b.
====================
*/
inline float V3dDot( register const unk_RwV3d* a, register const unk_RwV3d* b ) {
    register float dot;
    asm {
        psq_l       fp0, 0(a), 0, 0
        psq_l       fp2, 0(b), 0, 0
        psq_l       fp1, 8(a), 1, 0
        ps_mul      fp0, fp0, fp2
        psq_l       fp3, 8(b), 1, 0
        fmuls       fp1, fp1, fp3
        ps_sum0     fp2, fp0, fp0, fp0
        fadds       dot, fp1, fp2
    }
    return dot;
}

/*
 * A vector with the game's paired-single arithmetic. The copies are member-wise (float loads and
 * stores, not word copies). Name invented.
 */
class unk_Vector3 : public unk_RwV3d {
public:
                    unk_Vector3() {}
                    unk_Vector3( float vx, float vy, float vz ) { x = vx; y = vy; z = vz; }
                    unk_Vector3( const unk_RwV3d& v ) { x = v.x; y = v.y; z = v.z; }
                    unk_Vector3( const unk_Vector3& v ) { x = v.x; y = v.y; z = v.z; }

    unk_Vector3&    operator=( const unk_RwV3d& v ) { x = v.x; y = v.y; z = v.z; return *this; }

    unk_Vector3     operator*( float s ) const {
        unk_RwV3d result;
        V3dScale( &result, this, s );
        return result;
    }

    unk_Vector3     operator+( const unk_RwV3d& v ) const {
        unk_Vector3 result;
        result.x = x + v.x;
        result.y = y + v.y;
        result.z = z + v.z;
        return result;
    }

    unk_Vector3     operator-( const unk_RwV3d& v ) const {
        unk_Vector3 result;
        result.x = x - v.x;
        result.y = y - v.y;
        result.z = z - v.z;
        return result;
    }

    float           Dot( const unk_RwV3d& v ) const {
        return V3dDot( this, &v );
    }

    unk_Vector3&    operator-=( const unk_RwV3d& v ) {
        V3dSub( this, this, &v );
        return *this;
    }

    unk_Vector3&    operator*=( float s ) {
        V3dScale( this, this, s );
        return *this;
    }

    unk_Vector3&    operator+=( const unk_RwV3d& v ) {
        V3dAdd( this, this, &v );
        return *this;
    }
};

#endif
