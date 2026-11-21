// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_80022200.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>
extern "C" int fn_801B6138( const char* a, const char* b );
extern "C" void fn_801B68E4( char* dst, const char* src, int size );
#include <game/unk_8000F80C.h>

// Data owned by this unit (declare each once in the header, define it here; names are the
// symbols.txt names):
//   .sdata2 0x8052B820 size 0x8 lbl_8052B820
//   .sdata2 0x8052B828 size 0x4 lbl_8052B828
//   .sdata2 0x8052B82C size 0x4 lbl_8052B82C

/*
====================
fn_80022200
====================
*/
// Delta-morph update of one atomic: every target whose weight changed is locked, its position, normal,
// pre-light and texture-coordinate deltas are added to the base geometry (run-length coded), then the
// geometry is unlocked. The weight is stored as current either way. Name invented.
// Not exact: the register assignment and the block order of the four section loops differ from retail.
// 0x80022200
extern "C" void fn_80022200( RpAtomic* atomic ) {
    rpDMorphAtomicData* atomicData = RPDMORPHATOMICGETDATA( atomic );
    RpGeometry* geometry = atomic->geometry;
    rpDMorphGeometryData* geomData = RPDMORPHGEOMETRYGETDATA( geometry );
    if ( geomData == NULL || atomicData->values == NULL ) {
        return;
    }

    RpMorphTarget* baseTarget = geometry->morphTarget;
    RwV3d* baseVerts = baseTarget->verts;
    RwV3d* baseNormals = baseTarget->normals;
    RwRGBA* baseColors = geometry->preLitLum;
    RwTexCoords* baseTexCoords = geometry->texCoords[0];

    for ( u32 i = 0; i < geomData->numDMorphTargets; i++ ) {
        RwReal oldValue = geomData->currValues[i];
        RwReal newValue = atomicData->values[i];
        if ( newValue != oldValue ) {
            RwReal delta = newValue - oldValue;
            RpDMorphTarget* target = &geomData->dMorphTargets[i];
            RwV3d* deltaVerts = target->vertices;
            RwV3d* deltaNormals = target->normals;
            RwRGBA* deltaColors = target->preLightColors;
            RwTexCoords* deltaTexCoords = target->texCoords;

            RpGeometryLock( geometry, target->lockFlags );

            if ( target->flags & rpGEOMETRYPOSITIONS ) {
                u32 vertex = 0;
                for ( u32 run = 0; run < target->rle.numCodeElements; run++ ) {
                    u8 code = target->rle.code[run];
                    if ( code & 0x80 ) {
                        RwV3d* dst = &baseVerts[vertex];
                        for ( u8 n = 0; n < ( target->rle.code[run] & 0x7F ); n++ ) {
                            dst->x += delta * deltaVerts->x;
                            dst->y += delta * deltaVerts->y;
                            dst->z += delta * deltaVerts->z;
                            deltaVerts++;
                            dst++;
                            vertex++;
                        }
                    } else {
                        vertex += code & 0x7F;
                    }
                }
            }

            if ( target->flags & rpGEOMETRYNORMALS ) {
                u32 vertex = 0;
                for ( u32 run = 0; run < target->rle.numCodeElements; run++ ) {
                    u8 code = target->rle.code[run];
                    if ( code & 0x80 ) {
                        RwV3d* dst = &baseNormals[vertex];
                        for ( u8 n = 0; n < ( target->rle.code[run] & 0x7F ); n++ ) {
                            dst->x += delta * deltaNormals->x;
                            dst->y += delta * deltaNormals->y;
                            dst->z += delta * deltaNormals->z;
                            deltaNormals++;
                            dst++;
                            vertex++;
                        }
                    } else {
                        vertex += code & 0x7F;
                    }
                }
            }

            if ( target->flags & rpGEOMETRYPRELIT ) {
                u32 vertex = 0;
                for ( u32 run = 0; run < target->rle.numCodeElements; run++ ) {
                    u8 code = target->rle.code[run];
                    if ( code & 0x80 ) {
                        RwRGBA* color = &baseColors[vertex];
                        for ( u8 n = 0; n < ( target->rle.code[run] & 0x7F ); n++ ) {
                            color->alpha -= (u8)(s32)( oldValue * deltaColors->alpha );
                            color->blue -= (u8)(s32)( oldValue * deltaColors->blue );
                            color->green -= (u8)(s32)( oldValue * deltaColors->green );
                            color->red -= (u8)(s32)( oldValue * deltaColors->red );
                            color->alpha += (u8)(s32)( newValue * deltaColors->alpha );
                            color->blue += (u8)(s32)( newValue * deltaColors->blue );
                            color->green += (u8)(s32)( newValue * deltaColors->green );
                            color->red += (u8)(s32)( newValue * deltaColors->red );
                            deltaColors++;
                            color++;
                            vertex++;
                        }
                    } else {
                        vertex += code & 0x7F;
                    }
                }
            }

            if ( target->flags & rpGEOMETRYTEXTURED ) {
                u32 vertex = 0;
                for ( u32 run = 0; run < target->rle.numCodeElements; run++ ) {
                    u8 code = target->rle.code[run];
                    if ( code & 0x80 ) {
                        RwTexCoords* dst = &baseTexCoords[vertex];
                        for ( u8 n = 0; n < ( target->rle.code[run] & 0x7F ); n++ ) {
                            dst->u += delta * deltaTexCoords->u;
                            dst->v += delta * deltaTexCoords->v;
                            deltaTexCoords++;
                            dst++;
                            vertex++;
                        }
                    } else {
                        vertex += code & 0x7F;
                    }
                }
            }

            RpGeometryUnlock( geometry );
        }
        geomData->currValues[i] = atomicData->values[i];
    }
}

/*
====================
fn_8002273C
====================
*/
// 0x8002273C
extern "C" unk_80022200Task* fn_8002273C( unk_80022200Task* self, unk_80022200Task* other ) {
    if ( self != NULL && other != NULL && other->field_0xC == 0 ) {
        other->field_0x10 = 3;
        if ( other->field_0x20 != NULL ) {
            other->field_0x20( other, other->field_0x14, other->field_0x24 );
        }
    }
    return self;
}

/*
====================
fn_800227AC
====================
*/
// 0x800227AC
extern "C" void fn_800227AC( unk_80022200Task* self, RpClump* owner ) {
    self->field_0x0 = NULL;
    self->field_0x4 = NULL;
    self->field_0x8 = NULL;
    self->field_0x1C = owner;
    self->field_0xC = 0;
    self->field_0x10 = 0;
    self->field_0x14 = -1;
    self->field_0x18 = 1.0f;
    self->field_0x20 = NULL;
    self->field_0x24 = NULL;
    self->field_0x28 = 0.0f;
    self->field_0x2C = 0.0f;
    self->field_0x30 = 0.0f;
    self->field_0x34 = NULL;
}

/*
====================
fn_800227F8
====================
*/
// Destroys the elements of [first, last) in place. The elements are pointers, so there is no
// destructor to call and only the loop remains.
template <typename T>
static void DestroyRange( T* first, T* last ) {
    for ( ; first != last; ++first ) {
    }
}

// Not exact (86%): the element-delete loop and both destroy loops match; retail keeps the array base
// in r4 through the delete loop where this build keeps it in r5 (register assignment).
// 0x800227F8
extern "C" void fn_800227F8( unk_80022200Task* self, s32 deleteThis ) {
    if ( self != NULL ) {
        if ( self->field_0x34 != NULL ) {
            delete[] self->field_0x34;
            self->field_0x34 = NULL;
            self->field_0x30 = 0.0f;
        }
        for ( u32 i = 0; i < (u32)( self->field_0x4 - self->field_0x0 ); i++ ) {
            delete self->field_0x0[i];
        }
        DestroyRange( self->field_0x0, self->field_0x4 );
        self->field_0x4 = self->field_0x0;
        DestroyRange( self->field_0x0, self->field_0x4 );
        if ( self->field_0x0 != NULL ) {
            delete[] self->field_0x0;
        }
        if ( deleteThis > 0 ) {
            delete self;
        }
    }
}

/*
====================
FindItemIndex
====================
*/
// Index of the first owned item whose name matches, or -1. Retail inlines this search twice in
// fn_80022900 (before the add and after it). Name invented.
static s32 FindItemIndex( unk_80022200Task* self, const char* name ) {
    s32 index = -1;
    if ( name != NULL && ( self->field_0x4 - self->field_0x0 ) != 0 ) {
        u32 hash = RWS::SHUtils::GetStringHashForceLower( name, 0x80 );
        for ( u32 i = 0; i < (u32)( self->field_0x4 - self->field_0x0 ); i++ ) {
            unk_80022200Item* item = self->field_0x0[i];
            if ( item->nameHash == hash && fn_801B6138( item->name, name ) ) {
                index = (s32)i;
                break;
            }
        }
    }
    return index;
}

/*
====================
fn_80022900
====================
*/
// Adds an item named `name` to the owned list unless one is already there; returns the index the
// item has in the list, or -1. Name invented. Not exact (78%): retail keeps a no-op destroy loop over
// the old elements and a null-guarded element copy in the grow path, and assigns the search registers
// differently; MWCC drops the loop here.
// 0x80022900
extern "C" s32 fn_80022900( unk_80022200Task* self, const char* name, s32 param ) {
    s32 result = -1;
    if ( name == NULL ) {
        return result;
    }

    if ( FindItemIndex( self, name ) == -1 ) {
        unk_80022200Item* item = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_80022200Item;
        item->param = param;
        fn_801B68E4( item->name, name, 0x80 );
        item->nameHash = RWS::SHUtils::GetStringHashForceLower( item->name, 0x80 );

        if ( item->atomicType == AtomicTags::Invalid ) {
            // The type is the text between the first and second '.' of the name.
            char* firstDot = strchr( item->name, '.' );
            char* secondDot = strchr( firstDot + 1, '.' );
            u32 baseLength = (u32)( firstDot - item->name );
            strncpy( item->baseName, item->name, baseLength );
            item->baseName[baseLength] = '\0';
            item->baseHash = RWS::SHUtils::GetStringHashForceLower( item->baseName, baseLength );
            item->atomicType = fn_8000C5C4( firstDot + 1, (int)( secondDot - ( firstDot + 1 ) ) );
        }
        item->atomic = AtomicTags::FindAtomicByType( self->field_0x1C, item->atomicType );

        u32 count = (u32)( self->field_0x4 - self->field_0x0 );
        if ( (u32)( self->field_0x8 - self->field_0x0 ) < count + 1 ) {
            unk_80022200Item** oldBegin = self->field_0x0;
            unk_80022200Item** oldEnd = self->field_0x4;
            unk_80022200Item** newBegin = new ( GAME_HEAP, GAME_HEAP_FLAGS ) unk_80022200Item*[ count + 1 ];
            unk_80022200Item** dst = newBegin;
            for ( unk_80022200Item** src = oldBegin; src != oldEnd; ) {
                *dst++ = *src++;
            }
            self->field_0x8 = newBegin + count + 1;
            self->field_0x4 = newBegin + count;
            if ( oldBegin != NULL ) {
                delete[] oldBegin;
            }
            self->field_0x0 = newBegin;
        }
        *self->field_0x4++ = item;
        result = FindItemIndex( self, name );
    }
    return result;
}

/*
====================
fn_80022C50
====================
*/
// 0x80022C50
extern "C" void fn_80022C50() {
    // TODO
}

/*
====================
fn_80022F1C
====================
*/
// 0x80022F1C
extern "C" void fn_80022F1C() {
    // TODO
}

/*
====================
fn_8002304C
====================
*/
// Selects the owned item whose id is `id`: the current one when it matches, else the first one found
// (fn_800231D4 makes it current). Returns true when an item with that id was found. Name invented.
// Not exact (62%): retail calls fn_800231D4 from inside a counted loop that indexes the array with its
// own offset; this build folds the index into a pointer walk.
// 0x8002304C
extern "C" bool fn_8002304C( unk_80022200Task* self, u32 id ) {
    bool found = false;
    u32 index = self->field_0x14;
    if ( index != -1 && self->field_0x0[ index ]->id == id ) {
        found = true;
    } else if ( id != -1 ) {
        s32 count = self->field_0x4 - self->field_0x0;
        for ( s32 i = 0; i < count; i++ ) {
            if ( self->field_0x0[i]->id == id ) {
                fn_800231D4( self, i );
                found = true;
                break;
            }
        }
    }
    return found;
}

/*
====================
fn_80023100
====================
*/
// 0x80023100
extern "C" void fn_80023100() {
    // TODO
}

/*
====================
fn_800231D4
====================
*/
// 0x800231D4
extern "C" void fn_800231D4( unk_80022200Task* self, u32 index ) {
    // TODO
}

/*
====================
fn_80023548
====================
*/
// Stores a value in the parameter of the owned item at `index`. Name invented.
// 0x80023548
extern "C" void fn_80023548( unk_80022200Task* self, u32 index, s32 value ) {
    self->field_0x0[index]->param = value;
}

/*
====================
fn_8002355C
====================
*/
// Removes the last owned item and deletes it; clears the task id when it is `id`. Name invented.
// Not exact (93%): retail reloads the end field for the new end before it computes the count; this build
// computes the count from one load of it (scheduling of the field reads).
// 0x8002355C
extern "C" bool fn_8002355C( unk_80022200Task* self, u32 id ) {
    unk_80022200Item** begin = self->field_0x0;
    s32 count = self->field_0x4 - begin;
    s32 lastIndex = count - 1;
    unk_80022200Item* last = begin[ lastIndex ];
    self->field_0x4--;
    delete last;
    if ( id == self->field_0x14 ) {
        self->field_0x14 = -1;
    }
    return true;
}

/*
====================
fn_800235D8
====================
*/
// Out-of-line copy of FindItemIndex (retail keeps this one as a call target). Name invented.
// Not exact (99%): retail keeps the loop index and the byte offset in the other two registers (register
// assignment only).
// 0x800235D8
extern "C" s32 fn_800235D8( unk_80022200Task* self, const char* name ) {
    return FindItemIndex( self, name );
}

/*
====================
fn_80023694
====================
*/
// 0x80023694
extern "C" void fn_80023694() {
    // TODO
}

/*
====================
fn_80023890
====================
*/
// 0x80023890
extern "C" void fn_80023890() {
    // TODO
}

/*
====================
fn_800239DC
====================
*/
// Stores the signal flag of the task (0 until signalled). Name invented.
// 0x800239DC
extern "C" void fn_800239DC( unk_80022200Task* self, u32 signalled ) {
    self->field_0xC = signalled;
}

/*
====================
fn_800239E4
====================
*/
// 0x800239E4
extern "C" void fn_800239E4() {
    // TODO
}

/*
====================
fn_80023B30
====================
*/
// 0x80023B30
extern "C" void fn_80023B30() {
    // TODO
}

/*
====================
fn_80023C64
====================
*/
// 0x80023C64
extern "C" void fn_80023C64() {
    // TODO
}

/*
====================
fn_80023DA0
====================
*/
// Stores the scale of the task. Name invented.
// 0x80023DA0
extern "C" void fn_80023DA0( unk_80022200Task* self, f32 scale ) {
    self->field_0x18 = scale;
}

/*
====================
fn_80023DA8
====================
*/
// Runs the current item: sets its animation, runs its update callback when dt is positive, then runs
// the delta-morph update on its atomic when it has one. Name invented.
// 0x80023DA8
extern "C" void fn_80023DA8( unk_80022200Task* self, f32 dt ) {
    u32 id = self->field_0x14;
    if ( id != -1 ) {
        unk_80022200Item* item = self->field_0x0[ id ];
        item->setAnimation( item->atomic, (RpDMorphAnimation*)item->param );
        if ( dt > 0.0f ) {
            item->updateCallback( item->param, item->atomic, dt );
        }
        if ( self->field_0x0[ self->field_0x14 ]->atomic != NULL ) {
            fn_80022200( self->field_0x0[ self->field_0x14 ]->atomic );
        }
    }
}

/*
====================
fn_80023E60
====================
*/
// 0x80023E60
extern "C" void fn_80023E60() {
    // TODO
}
