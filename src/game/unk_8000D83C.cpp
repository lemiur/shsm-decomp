// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <math.h>
#include <game/unk_8000D83C.h>
#include <game/VectorPS.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata2 0x8052B718 size 0x4 lbl_8052B718
//   .sdata2 0x8052B71C size 0x4 lbl_8052B71C
//   .sdata2 0x8052B720 size 0x4 lbl_8052B720
//   .sdata2 0x8052B724 size 0x4 lbl_8052B724
//   .sdata2 0x8052B728 size 0x4 lbl_8052B728
//   .sdata2 0x8052B72C size 0x4 lbl_8052B72C
//   .sdata2 0x8052B730 size 0x4 lbl_8052B730
//   .sdata2 0x8052B734 size 0x4 lbl_8052B734
//   .sdata2 0x8052B738 size 0x4 lbl_8052B738
//   .sdata2 0x8052B73C size 0x4 lbl_8052B73C
//   .sdata2 0x8052B740 size 0x8 lbl_8052B740

/*
====================
fn_8000D83C
====================
*/
// Segment against an axis-aligned box given as center and half extent. The segment direction is
// normalised; the slab test clips [0, length] to the box and reports the entry fraction through
// outT (when non-null). Returns false when the segment misses the box.
// Open (99.65%, register/schedule class): retail keeps bounds[i] in register f(10 - i) and stores the
// box in index order; the reconstruction's allocator numbers the box values differently, and the swap's
// temporary takes another register. Every instruction and constant is retail's.
// 0x8000D83C
extern "C" bool fn_8000D83C( const unk_RwV3d* start, const unk_RwV3d* end, const unk_RwV3d* center, const unk_RwV3d* halfExtent, float* outT ) {
    unk_RwV3d dir;
    float bounds[6];
    float tEnter;
    float tExit;
    float length;
    int axis;

    bounds[0] = center->x - halfExtent->x;
    bounds[3] = center->x + halfExtent->x;
    bounds[1] = center->y - halfExtent->y;
    bounds[4] = center->y + halfExtent->y;
    bounds[2] = center->z - halfExtent->z;
    bounds[5] = center->z + halfExtent->z;

    dir.x = end->x - start->x;
    dir.y = end->y - start->y;
    dir.z = end->z - start->z;
    length = RwV3dNormalize( &dir, &dir );

    tExit = length;
    tEnter = 0.0f;
    for ( axis = 0; axis < 3; axis++ ) {
        float absD = fabs( unk_AxisComponent( dir, axis ) );

        if ( absD < 1.1920929e-07f ) {
            // Parallel to this slab: the start must lie inside it.
            if ( unk_AxisComponent( *start, axis ) < bounds[axis] || unk_AxisComponent( *start, axis ) > bounds[axis + 3] ) {
                return false;
            }
        } else {
            float invD = 1.0f / unk_AxisComponent( dir, axis );
            float t1 = invD * ( bounds[axis] - unk_AxisComponent( *start, axis ) );
            float t2 = invD * ( bounds[axis + 3] - unk_AxisComponent( *start, axis ) );
            float tmp;

            if ( t1 > t2 ) {
                tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            tEnter = ( tEnter > t1 ) ? tEnter : t1;
            tExit = ( tExit < t2 ) ? tExit : t2;
            if ( tEnter > tExit ) {
                return false;
            }
        }
    }

    if ( outT != NULL ) {
        *outT = tEnter / length;
    }
    return true;
}

/*
====================
fn_8000DB14
====================
*/
// Moving point against a capped cylinder: the point moves from `from` to `to`, the cylinder runs
// from `base` to `top` with radius `radius`. Returns true when the point touches the cylinder during
// the move and writes the entry fraction through outT. Retail's base-end test (the last quadratic in
// the axial-zero branch) carries 2*t^2*|move|^2 where the geometry gives t^2*|move|^2; the bytes keep
// it, and it is the original's expression.
// Open: control flow is isomorphic; the residue is register/schedule on the sqrt path and the
// 0.0f constant reference order.
// 0x8000DB14
extern "C" bool fn_8000DB14( const unk_RwV3d* base, const unk_RwV3d* top, const unk_RwV3d* from, const unk_RwV3d* to, float radius, float* outT ) {
    unk_RwV3d axis;
    unk_RwV3d fromRel;
    unk_RwV3d move;
    float axisLenSq;
    float moveLenSq;
    float fromDotAxis;
    float moveDotAxis;
    float fromDotMove;
    float radiusTerm;
    float a;
    float b;
    float c;

    axis.x = top->x - base->x;
    axis.y = top->y - base->y;
    axis.z = top->z - base->z;
    fromRel.x = from->x - base->x;
    fromRel.y = from->y - base->y;
    fromRel.z = from->z - base->z;
    move.x = to->x - from->x;
    move.y = to->y - from->y;
    move.z = to->z - from->z;

    fromDotAxis = V3dDot( &fromRel, &axis );
    moveDotAxis = V3dDot( &move, &axis );
    axisLenSq = V3dDot( &axis, &axis );

    if ( fromDotAxis < 0.0f && fromDotAxis + moveDotAxis < 0.0f ) {
        return false;
    }
    if ( fromDotAxis > axisLenSq && fromDotAxis + moveDotAxis > axisLenSq ) {
        return false;
    }

    moveLenSq = V3dDot( &move, &move );
    fromDotMove = V3dDot( &fromRel, &move );
    a = axisLenSq * moveLenSq - moveDotAxis * moveDotAxis;
    radiusTerm = V3dDot( &fromRel, &fromRel ) - radius * radius;
    c = axisLenSq * radiusTerm - fromDotAxis * fromDotAxis;

    if ( fabs( a ) < 1.0e-5f ) {
        // The move is parallel to the axis: contact is decided by the start alone.
        if ( c > 0.0f ) {
            return false;
        }
        if ( fromDotAxis < 0.0f ) {
            *outT = -fromDotMove / moveLenSq;
        } else if ( fromDotAxis > axisLenSq ) {
            *outT = ( moveDotAxis - fromDotMove ) / moveLenSq;
        } else {
            *outT = 0.0f;
        }
        return true;
    }

    b = axisLenSq * fromDotMove - moveDotAxis * fromDotAxis;
    {
        float discriminant = b * b - a * c;
        float root;
        float t;
        float proj;

        if ( discriminant < 0.0f ) {
            return false;
        }
        root = sqrt( discriminant );
        t = ( -b - root ) / a;
        *outT = t;
        if ( t < 0.0f || t > 1.0f ) {
            return false;
        }

        proj = fromDotAxis + t * moveDotAxis;
        if ( proj < 0.0f ) {
            float t1;
            float d;

            if ( moveDotAxis > 0.0f ) {
                return false;
            }
            t1 = -fromDotAxis / moveDotAxis;
            *outT = t1;
            d = radiusTerm + ( 2.0f * t1 ) * ( fromDotMove + t1 * moveLenSq );
            return d <= 0.0f;
        }
        if ( proj > axisLenSq ) {
            float t2;
            float d;

            if ( moveDotAxis < 0.0f ) {
                return false;
            }
            t2 = ( axisLenSq - fromDotAxis ) / moveDotAxis;
            *outT = t2;
            d = ( radiusTerm + axisLenSq - 2.0f * fromDotAxis ) + t2 * ( 2.0f * ( fromDotMove - moveDotAxis ) + t2 * moveLenSq );
            return d <= 0.0f;
        }
    }
    return true;
}

/*
====================
fn_8000DED8
====================
*/
// Sweep of a vertical capsule (the axis runs from origin.y - halfHeight to origin.y + halfHeight,
// radius `radius`) against the record's motion from prevPos to pos. A position inside the capsule
// is recorded with zero time. Otherwise the motion is tested in the axis-normalised frame with
// fn_8000DB14; a hit earlier than the record's best time moves the contact point and the normal.
// Returns true when the record was written.
// 0x8000DED8
extern "C" bool fn_8000DED8( void* owner, const void* unused, const unk_RwV3d* origin, unk_SweepRecord* rec, float radius, float halfHeight ) {
    unk_RwV3d horizontal;
    unk_RwV3d base;
    unk_RwV3d top;
    unk_RwV3d from;
    unk_RwV3d to;
    unk_RwV3d step;
    unk_RwV3d move;
    float radiusSq;
    float t;
    float invHeight;
    bool inside;

    inside = rec->pos.y >= origin->y - halfHeight && rec->pos.y <= origin->y + halfHeight;
    if ( inside ) {
        horizontal.x = origin->x - rec->pos.x;
        horizontal.y = 0.0f;
        horizontal.z = origin->z - rec->pos.z;
        radiusSq = radius * radius;
        inside = V3dDot( &horizontal, &horizontal ) <= radiusSq;
    }
    if ( inside ) {
        rec->contact = rec->pos;
        rec->normal = rec->dir;
        rec->bestT = 0.0f;
        rec->owner = owner;
        return true;
    }

    invHeight = 1.0f / ( 2.0f * halfHeight );
    base.x = 0.0f;
    base.y = -0.5f;
    base.z = 0.0f;
    top.x = 0.0f;
    top.y = 0.5f;
    top.z = 0.0f;
    from.x = rec->pos.x - origin->x;
    from.y = ( rec->pos.y - origin->y ) * invHeight;
    from.z = rec->pos.z - origin->z;
    to.x = rec->prevPos.x - origin->x;
    to.y = ( rec->prevPos.y - origin->y ) * invHeight;
    to.z = rec->prevPos.z - origin->z;

    t = 0.0f;
    if ( !fn_8000DB14( &base, &top, &from, &to, radius, &t ) ) {
        return false;
    }
    if ( t >= rec->bestT ) {
        return false;
    }

    V3dScale( &step, &rec->dir, t );
    V3dScale( &move, &step, rec->scale );
    rec->contact.x = rec->pos.x + move.x;
    rec->contact.y = rec->pos.y + move.y;
    rec->contact.z = rec->pos.z + move.z;
    rec->normal.x = rec->contact.x - origin->x;
    rec->normal.y = 0.0f;
    rec->normal.z = rec->contact.z - origin->z;
    rec->bestT = t;
    V3dNormalize( &rec->normal );
    rec->owner = owner;
    return true;
}

/*
====================
fn_8000E244
====================
*/
// Stub: the body is not reconstructed yet (the retail function is 406 instructions: a shape-kind 5
// sweep that calls RwMatrixMultiply, RwMatrixInvert and fn_8000D83C). NonMatching until then.
// 0x8000E244
extern "C" void fn_8000E244( unk_RwpBody* body, unk_RwpShape* shape, unk_SweepRecord* rec ) {
    // TODO
}

/*
====================
fn_8000E89C
====================
*/
// Callback of RwpMgrForAllObjs: sweeps the record against one body. A flagged sweep skips the
// bodies whose bit is set in their world. Each part of the body is tested by its shape kind:
// kind 3 is a capsule, kind 5 goes to fn_8000E244, kind 6 is a cylinder-like shape; other kinds
// are not swept. Every part is posed by the body's matrix, the matrix of its first part.
// Open (88.6%): the flag test and the parts pointer take other registers than retail's, and the
// kind switch lowers to a compare tree with an extra range check (retail's kinds 7 to 10 test).
// 0x8000E89C
extern "C" void fn_8000E89C( unk_RwpBody* body, unk_SweepRecord* rec ) {
    bool skip = false;
    int i;

    if ( rec->flag ) {
        bool flagged = false;

        if ( body->world != NULL ) {
            if ( ( body->world->bodyBits[ body->index >> 5 ] & ( 1 << ( body->index & 31 ) ) ) != 0 ) {
                flagged = true;
            }
        }
        if ( flagged ) {
            skip = true;
        }
    }

    if ( body != NULL && rec != NULL && body != rec->ignored && !skip ) {
        for ( i = 0; i < body->partCount; i++ ) {
            unk_RwpBodyParts* parts = body->parts;
            unk_RwpShape* shape = parts[i].shape;

            if ( shape != NULL ) {
                switch ( shape->geometry->type ) {
                case 3: {
                    unk_RwMatrix m;
                    unk_RwV3d origin;
                    float radius;

                    RwMatrixMultiply( &m, ( shape->geometry->flags & 2 ) ? NULL : &shape->local, parts->matrix );
                    origin = m.pos;
                    radius = fn_802FDE64( shape );
                    fn_8000DED8( body, shape, &origin, rec, radius, radius + fn_802FDE6C( shape ) );
                    break;
                }
                case 6: {
                    unk_RwMatrix m;
                    unk_RwV3d origin;
                    float radius;

                    RwMatrixMultiply( &m, ( shape->geometry->flags & 2 ) ? NULL : &shape->local, parts->matrix );
                    origin = m.pos;
                    radius = fn_8030195C( shape );
                    fn_8000DED8( body, shape, &origin, rec, radius, fn_80301964( shape ) );
                    break;
                }
                case 5:
                    fn_8000E244( body, shape, rec );
                    break;
                }
            }
        }
    }
}

/*
====================
fn_8000EAB8
====================
*/
// Sweeps the segment from `from` to `to` through the manager of `owner`. On a hit it writes the
// contact point to `move` and the surface normal to `normalOut`. Returns true when the sweep hit a
// node.
// Open (66.9%): retail computes the segment in scalar floats with the zero constant held in a
// register, and keeps the record one slot higher on the stack; the register and schedule residue
// is not yet classified.
// 0x8000EAB8
extern "C" bool fn_8000EAB8( unk_SweepOwner* owner, const unk_RwV3d* from, const unk_RwV3d* to, unk_RwV3d* move, unk_RwV3d* normalOut, void* ignored, bool flag ) {
    unk_SweepRecord rec;
    void* list;
    void* mgr;

    if ( owner != NULL ) {
        if ( owner->field_0xc0 != NULL ) {
            list = *owner->field_0xc0;
        } else {
            list = NULL;
        }
    } else {
        list = NULL;
    }
    if ( list != NULL ) {
        mgr = *(void**)list;
    } else {
        mgr = NULL;
    }
    if ( mgr == NULL ) {
        return false;
    }

    rec.pos = *from;
    rec.prevPos = *to;
    rec.dir.x = to->x - from->x;
    rec.dir.y = to->y - from->y;
    rec.dir.z = to->z - from->z;
    rec.scale = V3dLength( &rec.dir );
    rec.contact.x = 0.0f;
    rec.contact.y = 0.0f;
    rec.contact.z = 0.0f;
    rec.normal.x = 0.0f;
    rec.normal.y = 0.0f;
    rec.normal.z = 0.0f;
    rec.bestT = 3.4028235e+38f;
    rec.ignored = ignored;
    rec.owner = NULL;
    rec.flag = flag;
    if ( rec.scale > 0.0f ) {
        V3dScale( &rec.dir, &rec.dir, 1.0f / rec.scale );
    }
    if ( rec.scale > 0.0f ) {
        RwpMgrForAllObjs( mgr, fn_8000E89C, &rec );
    }

    if ( rec.owner == NULL ) {
        return false;
    }
    *move = rec.contact;
    *normalOut = rec.normal;
    return true;
}
