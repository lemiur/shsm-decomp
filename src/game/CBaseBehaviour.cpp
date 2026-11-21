#include <game/CBaseBehaviour.h>
#include <game/unk_NamedEntryPlayer.h>
#include <game/CSystemCommands.h>
#include <climax/Utils.h>

/*
====================
fn_8009C95C__FRC19unk_AttributePacketb

Reads the commands of the packet addressed to CBaseBehaviour: ids 2, 4, 6 and 8 carry a name and
ids 3, 5, 7 and 9 the value that goes with it. Id 10 is a flag that stops the search when it is
zero; the caller's flag forces the search. Returns true when the first named entry the singleton
does not hold with that value. Name invented.
NonMatching: the control flow is isomorphic; the remaining wall is register/schedule (the
command pointer and the id loop's registers, the result's store order).
====================
*/
// 0x8009C95C
bool fn_8009C95C( const unk_AttributePacket& packet, bool flag ) {
    const char* names[4];
    u32         values[4];
    for ( int k = 0; k < 4; k++ ) {
        names[k] = NULL;
        values[k] = 0xFFFFFFFF;
    }
    bool        search = true;
    bool        result = flag;
    unk_AttributeCommandIterator it( packet, lbl_805297A8 );

    while ( !it.IsFinished() ) {
        const unk_AttributeCommand* cmd = it.current;
        if ( cmd->id == 10 ) {
            search = cmd->GetU32() == 0;
        } else {
            for ( int id = 2, k = 0; k < 4; k++, id += 2 ) {
                if ( cmd->id == id ) {
                    names[k] = cmd->GetString();
                    break;
                }
                if ( cmd->id == id + 1 ) {
                    values[k] = cmd->GetU32();
                    break;
                }
            }
        }
        ++it;
    }

    if ( result ) {
        search = true;
    }
    result = false;
    if ( search ) {
        for ( int k = 0; k < 4 && !result; k++ ) {
            if ( names[k] != NULL && *names[k] != '\0' && values[k] != 0xFFFFFFFF ) {
                u32 value;
                if ( lbl_8052A0D0->fn_8014F114( names[k], &value ) == 1 ) {
                    result = value != values[k];
                }
            }
        }
    }
    return result;
}

/*
====================
CBaseBehaviour::fn_8009CB1C

The base handler of event messages does nothing.
====================
*/
// 0x8009CB1C
void CBaseBehaviour::fn_8009CB1C( unk_Msg& ) {
}

/* Copies a vector word by word into a game vector (unk_Vec3 has a copy constructor, not an assignment from RwV3d). */
static inline void CopyVec3( unk_Vec3* out, const unk_RwV3d& v ) {
    out->x = v.x;
    out->y = v.y;
    out->z = v.z;
}

/*
 * HandleAttributes. The system SetMatrix command replaces the frame's modelling matrix, then the
 * behaviour's own id-0 command (class id lbl_805297A8) names the handler's filter string.
 */
// 0x8009CB20
void CBaseBehaviour::fn_801A73A8( const unk_AttributePacket& packet ) {
    for ( unk_AttributeCommandIterator it( packet, lbl_805297E8 ); !it.IsFinished(); ++it ) {
        if ( it->id == SYSCMD_SetMatrix ) {
            unk_RwMatrix* matrix = RwFrameGetMatrix( field_0x30 );
            *matrix = *( const unk_RwMatrix* )it->data;
            RwMatrixOptimize( matrix, NULL );
            RwFrameUpdateObjects( field_0x30 );
        }
    }
    for ( unk_AttributeCommandIterator it( packet, lbl_805297A8 ); !it.IsFinished(); ++it ) {
        if ( it->id == 0 ) {
            const char* name = it->GetString();
            delete mFilter;
            mFilter = fn_801B67FC( name, 0 );
        }
    }
}

/*
 * Writes the position of the frame's matrix and updates the matrix. Retail has two copies of this
 * body (0x8009CCA0 and 0x8009CCC4); the owning class of the first is not yet known, so it is a
 * C-linkage accessor on the untyped object.
 */
// 0x8009CCA0
void fn_8009CCA0( void* self, const unk_RwV3d* position ) {
    CBaseBehaviour* behaviour = static_cast<CBaseBehaviour*>( self );
    unk_RwMatrix* matrix = RwFrameGetMatrix( behaviour->field_0x30 );
    matrix->pos = *position;
    RwMatrixUpdate( matrix );
}

// 0x8009CCC4
void fn_8009CCC4( void* self, const unk_RwV3d* position ) {
    CBaseBehaviour* behaviour = static_cast<CBaseBehaviour*>( self );
    unk_RwMatrix* matrix = RwFrameGetMatrix( behaviour->field_0x30 );
    matrix->pos = *position;
    RwMatrixUpdate( matrix );
}

// 0x8009CCE8
void CBaseBehaviour::fn_8009CCE8( unk_Vec3* out ) const {
    CopyVec3( out, field_0x30->modelling.pos );
}

// 0x8009CD08
const unk_RwV3d* CBaseBehaviour::fn_8009CD08() const {
    return &field_0x30->modelling.pos;
}

// 0x8009CD14
const unk_RwV3d* fn_8009CD14( const void* self ) {
    return &static_cast<const CBaseBehaviour*>( self )->field_0x30->modelling.pos;
}

// 0x8009CD20
const unk_RwV3d* CBaseBehaviour::fn_8009CD20() const {
    return &field_0x30->modelling.right;
}

// 0x8009CD2C
void CBaseBehaviour::fn_8009CD2C( unk_Vec3* out ) const {
    CopyVec3( out, field_0x30->modelling.right );
}

// 0x8009CD4C
const unk_RwV3d* CBaseBehaviour::fn_8009CD4C() const {
    return &field_0x30->modelling.up;
}

// 0x8009CD58
void fn_8009CD58( const void* self, unk_Vec3* out ) {
    CopyVec3( out, static_cast<const CBaseBehaviour*>( self )->field_0x30->modelling.up );
}

// 0x8009CD78
const unk_RwV3d* CBaseBehaviour::fn_8009CD78() const {
    return &field_0x30->modelling.at;
}

// 0x8009CD84
void CBaseBehaviour::fn_8009CD84( unk_Vec3* out ) const {
    CopyVec3( out, field_0x30->modelling.at );
}

// 0x8009CDA4
void CBaseBehaviour::fn_8009CDA4( unk_RwMatrix* matrix ) const {
    *matrix = field_0x30->modelling;
}

// 0x8009CE2C
void CBaseBehaviour::fn_8009CE2C( const void* matrix ) {
    unk_RwMatrix* dst = RwFrameGetMatrix( field_0x30 );
    *dst = *( const unk_RwMatrix* )matrix;
    RwMatrixOptimize( dst, NULL );
    RwFrameUpdateObjects( field_0x30 );
}
