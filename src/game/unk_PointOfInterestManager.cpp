// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_PointOfInterestManager.h>
#include <game/CPointOfInterest.h>
#include <climax/Memory.h>
#include <climax/Utils.h>
#include <climax/EventHandler.h>
#include <game/CAmbientInterferencePoint.h>
#include <game/CPlayer.h>
#include <game/unk_800E671C.h>
#include <game/unk_80114598.h>
#include <math.h>

#define FLOAT_MAX       3.4028235e+38f

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .data 0x803EBAE8 size 0x10 lbl_803EBAE8 (the vtable of the list at 0x40C)
//   .sdata2 0x8052C718..0x8052C738: the constants of the manager's constructor and of fn_800C5800

/*
====================
fn_800C5604

Creates the point manager.
====================
*/
// 0x800C5604
void fn_800C5604() {
    lbl_80529A88 = new( GAME_HEAP, GAME_HEAP_FLAGS ) unk_PointOfInterestManager;
}

/*
====================
unk_PointOfInterestManager::fn_800C56A4

Files poi, unless it is filed already.
====================
*/
// 0x800C56A4
void unk_PointOfInterestManager::fn_800C56A4( CPointOfInterest* poi ) {
    if ( IndexOf( poi ) < 0 ) {
        points[pointCount] = poi;
        pointCount++;
    }
}

/*
====================
unk_PointOfInterestManager::fn_800C5704

Removes poi, moving the last point into its place, and forgets it as the chosen point.
====================
*/
// 0x800C5704
void unk_PointOfInterestManager::fn_800C5704( CPointOfInterest* poi ) {
    int index = IndexOf( poi );
    if ( index >= 0 ) {
        int last = pointCount - 1;
        if ( last != index ) {
            points[index] = points[last];
        }
        points[last] = NULL;
        pointCount--;
        if ( nearestInView == poi ) {
            nearestInView = NULL;
        }
        if ( chosen == poi ) {
            chosen = NULL;
        }
    }
}

/*
====================
unk_PointOfInterestManager::fn_800C57A8

Whether to lies within the height window around from.
====================
*/
// 0x800C57A8
bool unk_PointOfInterestManager::fn_800C57A8( const unk_RwV3d* from, const unk_RwV3d* to ) {
    float dy = to->y - from->y;
    return dy >= heightMin && dy <= heightMax;
}

/*
====================
unk_PointOfInterestManager::fn_800C57E4

Whether distance is inside the range of poi, with the margin.
====================
*/
// 0x800C57E4
bool unk_PointOfInterestManager::fn_800C57E4( const CPointOfInterest* poi, float distance ) {
    float range = poi->field_0x20;
    return distance < rangeMargin + range;
}

/*
====================
unk_PointOfInterestManager::fn_800C5800

Chooses the points the player's view picks: the nearest in view (nearestInView) and the one with
the highest priority (chosen).

Up to three view directions: the player's, the current camera's at-vector when the camera says
so (only CPlayerCamera does), and the camera's (the lbl_8052A120 query) when the player asks for
it. A point counts when it is active, in the player's zone, and inside the height window (or, for
points that allow it, within field_0x34 of the player's height); its flattened direction is
compared with every view direction.

position is const: retail keeps its components in registers across the loop's virtual calls,
which MWCC does only for a const object. fn_8009CD84 takes a unk_Vec3*, hence the cast onto the
direction array (unk_Vector3, which V3dDot needs); CDrawingTest passes it unk_Vec3s, so it keeps
that type.

Residue (register allocation, 99.74%): the camera takes r21 (retail r25, poi's register) and the
height test's fabsf and field_0x34 swap f6/f7. Tried without effect: the camera declared
uninitialised before directionCount, the two tests as nested ifs; GetCamera as two if-statements
is worse (here and in CPsyPointOfInterest).
====================
*/
// 0x800C5800
void unk_PointOfInterestManager::fn_800C5800( CPlayer* player ) {
    const unk_Vector3 position = *player->fn_8009CD08();
    unk_Vector3 directions[3];
    int directionCount = 0;
    directions[directionCount++] = player->field_0x3B4;
    unk_80114598* camera = lbl_80529E44->GetCamera();
    if ( camera != NULL && camera->fn_800B5B78() ) {
        camera->fn_8009CD84( reinterpret_cast< unk_Vec3* >( &directions[directionCount++] ) );
    }
    if ( player->field_0x128C ) {
        unk_Msg query( lbl_8052A120, NULL );
        if ( query.record != lbl_8052A530 && query.record->head.next != NULL ) {
            fn_801AA2AC( query );
        }
        unk_CameraInfo* cameraInfo = static_cast< unk_CameraInfo* >( query.data );
        if ( cameraInfo != NULL ) {
            directions[directionCount++] = cameraInfo->direction;
        }
    }

    float nearestDistance = FLOAT_MAX;
    int chosenIndex = -1;
    int bestPriority = -11;
    int nearestIndex = -1;
    for ( int i = 0; i < pointCount; i++ ) {
        CPointOfInterest* poi = points[i];
        if ( !poi->fn_800C54EC() || poi->fn_800C4E8C() != player->GetZone() ) {
            continue;
        }
        unk_Vector3 nearest = poi->fn_800C5280( player );
        unk_Vector3 toPoint = nearest;
        if ( fn_800C57A8( &position, &nearest ) ||
             ( poi->fn_8002CDC8() && poi->field_0x1D && fabsf( toPoint.y - position.y ) < poi->field_0x34 ) ) {
            unk_Vector3 flatPosition = position;
            flatPosition.y = 0.0f;
            toPoint.y = 0.0f;
            toPoint -= flatPosition;
            float distance = V3dLength( &toPoint );
            V3dNormalize( &toPoint );
            for ( int j = 0; j != directionCount; j++ ) {
                float dot = V3dDot( &toPoint, &directions[j] );
                if ( poi->fn_8002CDC8() && distance <= poi->field_0x34 &&
                     ( dot > viewDotWide || poi->field_0x1D ) && poi->field_0x30 > bestPriority ) {
                    chosenIndex = i;
                    bestPriority = poi->field_0x30;
                }
                if ( poi->fn_8002CDF0() && fn_800C57E4( poi, distance ) && fn_800C57A8( &position, &nearest ) &&
                     dot > viewDotNarrow && distance < nearestDistance ) {
                    nearestDistance = distance;
                    nearestIndex = i;
                }
            }
        }
    }

    if ( nearestIndex >= 0 ) {
        nearestInView = points[nearestIndex];
    } else {
        nearestInView = NULL;
    }
    if ( nearestInView != NULL && nearestInView->fn_8002CDC8() ) {
        chosen = nearestInView;
    } else if ( chosenIndex >= 0 ) {
        chosen = points[chosenIndex];
    } else {
        chosen = NULL;
    }
}

/*
====================
unk_PointOfInterestManager::fn_800C5D60

Activates the nearest point in view for who.
====================
*/
// 0x800C5D60
bool unk_PointOfInterestManager::fn_800C5D60( unk_8003C454* who ) {
    if ( nearestInView != NULL ) {
        nearestInView->fn_800C6274( who );
        return true;
    }
    return false;
}

/*
====================
unk_PointOfInterestManager::fn_800C5DA4

Activates the nearest point in view for who, when it allows it (fn_8005230C).
====================
*/
// 0x800C5DA4
bool unk_PointOfInterestManager::fn_800C5DA4( unk_8003C454* who ) {
    if ( nearestInView != NULL && nearestInView->fn_8005230C() ) {
        nearestInView->fn_800C6274( who );
        return true;
    }
    return false;
}

/*
====================
unk_PointOfInterestManager::fn_800C5E24

The first filed point.
====================
*/
// 0x800C5E24
CPointOfInterest* unk_PointOfInterestManager::fn_800C5E24() {
    return points[0];
}

/*
====================
unk_PointOfInterestManager::fn_800C5E2C

The point filed after poi, NULL for the last.
====================
*/
// 0x800C5E2C
CPointOfInterest* unk_PointOfInterestManager::fn_800C5E2C( CPointOfInterest* poi ) {
    CPointOfInterest* next = NULL;
    int index = IndexOf( poi ) + 1;
    if ( index < pointCount ) {
        next = points[index];
    }
    return next;
}

/*
====================
unk_PointOfInterestManager::fn_800C5E8C

The next point after poi that has the same name.

Residue (register allocation): the shared FindByName (which closes fn_800C5F4C) gives retail's
index (r31), but found and the hoisted name are swapped (retail found r30, name r29; this build
name r30, found r29), and retail loads pointCount for IndexOf before the stmw. Tried: name or the
start index as named locals first, an open-coded loop with found declared before name,
FindByName's parameters in the other order; all worse or unchanged. Retail numbers found before the name, as if the argument were not bound to a
temporary ahead of the inline's own local.
====================
*/
// 0x800C5E8C
CPointOfInterest* unk_PointOfInterestManager::fn_800C5E8C( CPointOfInterest* poi ) {
    return FindByName( IndexOf( poi ) + 1, poi->name );
}

/*
====================
unk_PointOfInterestManager::fn_800C5F4C

The first point whose name is name.
====================
*/
// 0x800C5F4C
CPointOfInterest* unk_PointOfInterestManager::fn_800C5F4C( const char* name ) {
    return FindByName( 0, name );
}
