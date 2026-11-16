// unit name unconfirmed: the class name is from the registration strings and the log strings
// "PlaneTrigger[%s] Sends %s (Event1)" and "(Event2)"; the file name is invented.
// The inline factory fn_800CE5C8 and the two thunks follow the static initialiser in .text.

#include <game/PlaneTrigger.h>
#include <climax/Frame.h>
#include <game/AreaTriggerBase.h>
#include <game/CSystemCommands.h>
#include <game/Log.h>
#include <game/Messages.h>
#include <game/unk_800C8598.h>

/*
====================
DistanceAlong

How far point lies from origin along axis: with a unit normal, the signed distance from the plane
through origin. Name invented.
====================
*/
static inline float DistanceAlong( const unk_RwV3d& point, const unk_RwV3d& origin, const unk_RwV3d& axis ) {
    unk_RwV3d offset;
    offset.x = point.x - origin.x;
    offset.y = point.y - origin.y;
    offset.z = point.z - origin.z;
    return offset.x * axis.x + offset.y * axis.y + offset.z * axis.z;
}

/*
====================
PlaneTrigger::~PlaneTrigger
====================
*/
// 0x800CDD50
PlaneTrigger::~PlaneTrigger() {
    if ( triggerMsg != NULL ) {
        fn_801A9E00( *triggerMsg );
    }
    fn_801A9940( event1 );
    fn_801A9940( event2 );
    fn_801A9E00( enableMsg );
    fn_801A9940( enableMsg );
    fn_801A9E00( disableMsg );
    fn_801A9940( disableMsg );
    fn_801A9E00( lbl_8052A788 );
}

/*
====================
PlaneTrigger::fn_8009CB1C

Follows the point of the trigger message through the gate and sends Event1 or Event2 when it
leaves by the plane opposite the one it came in by. The enable and disable messages switch the
trigger.
====================
*/
// 0x800CDE50
void PlaneTrigger::fn_8009CB1C( unk_Msg& msg ) {
    CBaseBehaviour::fn_8009CB1C( msg );
    // the record read through the accessor: retail loads it before msg.record, as for a call
    if ( triggerMsg != NULL && msg.record == triggerMsg->GetRecord() ) {
        unk_ColdMsgData* data = ( unk_ColdMsgData* )msg.data;
        if ( data != NULL && data->zone == CAttributeHandler::mZone ) {
            unk_RwV3d point = data->pos;
            const unk_RwV3d* centre = fn_8009CD08();
            float newDistA = DistanceAlong( point, pointA, normalA );
            float newDistB = DistanceAlong( point, pointB, normalB );
            if ( !enabled || !tracking ) {
                tracking = true;
            } else {
                float height = DistanceAlong( point, *centre, upAxis );
                float depth = DistanceAlong( point, *centre, atAxis );
                if ( depth < halfDepth && depth > -halfDepth && height < halfHeight && height > -halfHeight ) {
                    if ( distA >= 0.0f && newDistA < 0.0f ) {
                        pendingEvent2 = true;
                    } else if ( distB >= 0.0f && newDistB < 0.0f ) {
                        pendingEvent1 = true;
                    }
                    if ( pendingEvent1 && distA < 0.0f && newDistA >= 0.0f ) {
                        unk_Msg event( event1, NULL );
                        fn_800CE238( event );
                        fn_80028D0C( 4, "PlaneTrigger[%s] Sends %s (Event1)", NULL,
                                     event1.record != NULL ? event1.record->name : NULL );
                        if ( event.record != lbl_8052A530.record && event.record->head.next != NULL ) {
                            fn_801AA2AC( event );
                        }
                        pendingEvent1 = false;
                    } else if ( pendingEvent2 && distB < 0.0f && newDistB >= 0.0f ) {
                        unk_Msg event( event2, NULL );
                        fn_800CE238( event );
                        fn_80028D0C( 4, "PlaneTrigger[%s] Sends %s (Event2)", NULL,
                                     event2.record != NULL ? event2.record->name : NULL );
                        if ( event.record != lbl_8052A530.record && event.record->head.next != NULL ) {
                            fn_801AA2AC( event );
                        }
                        pendingEvent2 = false;
                    }
                }
            }
            distA = newDistA;
            distB = newDistB;
        }
    }
    if ( msg.record == enableMsg.record ) {
        enabled = true;
    }
    if ( msg.record == disableMsg.record ) {
        enabled = false;
    }
}

/*
====================
PlaneTrigger::fn_800CE238
====================
*/
// 0x800CE238
void PlaneTrigger::fn_800CE238( unk_Msg& msg ) {}

/*
====================
PlaneTrigger::fn_801A73A8

Builds the two planes and the gate's extent from the frame when the packet places it, then takes
the messages, the trigger message and the enabled state from the packet. At the first loop's
++sys the command pointer loaded for sys->id is reused: operator++ takes the next command from
the iterator's const Next(), whose read of current carries the same const qualifier.
====================
*/
// 0x800CE23C
void PlaneTrigger::fn_801A73A8( const unk_AttributePacket& packet ) {
    CBaseBehaviour::fn_801A73A8( packet );
    unk_AttributeCommandIterator sys( packet, lbl_805297E8 );
    while ( !sys.IsFinished() ) {
        if ( sys->id == SYSCMD_SetMatrix ) {
            const unk_RwV3d* right = fn_8009CD20();
            unk_RwV3d halfRight;
            halfRight.x = -0.5f * right->x;
            halfRight.y = -0.5f * right->y;
            halfRight.z = -0.5f * right->z;
            const unk_RwV3d* pos = fn_8009CD08();
            pointA.x = pos->x + halfRight.x;
            pointA.y = pos->y + halfRight.y;
            pointA.z = pos->z + halfRight.z;
            RwV3dNormalize( &normalA, &halfRight );
            pointB.x = pos->x - halfRight.x;
            pointB.y = pos->y - halfRight.y;
            pointB.z = pos->z - halfRight.z;
            normalB.x = -normalA.x;
            normalB.y = -normalA.y;
            normalB.z = -normalA.z;
            const unk_RwV3d* up = fn_8009CD4C();
            const unk_RwV3d* at = fn_8009CD78();
            halfHeight = 0.5f * RwV3dLength( up );
            halfDepth = 0.5f * RwV3dLength( at );
            RwV3dNormalize( &upAxis, up );
            RwV3dNormalize( &atAxis, at );
            break;
        }
        ++sys;
    }

    unk_AttributeCommandIterator it( packet, lbl_80529BA4 );
    while ( !it.IsFinished() ) {
        switch ( it->id ) {
        case CMD_SetEnabled:
            enabled = it->GetU32() != 0;
            break;
        case CMD_SetEnableMsg:
            fn_801AA218( enableMsg, it->GetString(), 0 );
            break;
        case CMD_SetDisableMsg:
            fn_801AA218( disableMsg, it->GetString(), 0 );
            break;
        case CMD_SetTriggerType:
            if ( triggerMsg != NULL ) {
                fn_801A9E00( *triggerMsg );
                triggerMsg = NULL;
            }
            switch ( it->GetU32() ) {
            case TRIGGER_Player:
                triggerMsg = &lbl_80529C08;
                break;
            case TRIGGER_Enemy:
                triggerMsg = &lbl_80529C10;
                break;
            case TRIGGER_Boss:
                triggerMsg = &lbl_80529C18;
                break;
            case TRIGGER_RaceStartLine:
                triggerMsg = &lbl_80529C20;
                break;
            }
            if ( triggerMsg != NULL ) {
                fn_801A9DE8( *triggerMsg );
            }
            break;
        case CMD_SetEvent1:
            fn_801AA1C4( event1, it->GetString(), 0 );
            break;
        case CMD_SetEvent2:
            fn_801AA1C4( event2, it->GetString(), 0 );
            break;
        }
        ++it;
    }
}

unk_801A8F6C lbl_80529BA0( "PlaneTrigger", PlaneTrigger::fn_800CE5C8, sizeof( PlaneTrigger ) );
u32 lbl_80529BA4 = fn_801A7A84( "PlaneTrigger" );
