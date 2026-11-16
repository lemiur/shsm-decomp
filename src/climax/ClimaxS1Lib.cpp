// Unit name and boundary: Gormiti link map (ClimaxS1Lib.obj).
// Names and layout: Gormiti link map and DWARF (ClimaxS1Lib.obj).
//
// The ClimaxS1 library: the arrays of integers and floats the effects read their parameters from. This
// unit holds the reading of a data block from a stream and the accessors of the stored arrays; the
// builder functions of the Gormiti object are not in retail.

#include <climax/ClimaxS1Lib.h>
#include <rwsdk/plcore/bamemory.h>

/*
====================
ClimaxS1DataStreamRead

Reads a data block: its byte size, the header (number of arrays and type), the table of array offsets and
then the arrays, each with its count and element type. Returns NULL when the stream fails. The block is
walked as bytes (char* data and temp, as in Gormiti's DWARF), and temp is stepped past the header and the
offset table before imax is set, in the statement order of Gormiti's Debug build.
====================
*/
// 0x801D731C
char* ClimaxS1DataStreamRead( RwStream* stream ) {
    int   i;
    int   imax;
    int   size;
    int   type;
    char* data;
    char* temp;

    if ( !RwStreamReadInt32( stream, &size, 4 ) ) {
        return NULL;
    }

    data = (char*)RwOsGetMemoryInterface()->rwmalloc( size, 0 );
    if ( data ) {
        if ( !RwStreamReadInt32( stream, &size, 4 ) ) {
            return NULL;
        }
        ( (int*)data )[0] = size;
        if ( !RwStreamReadInt32( stream, &type, 4 ) ) {
            return NULL;
        }
        ( (int*)data )[1] = type;
        if ( !RwStreamReadInt32( stream, (int*)( data + CLIMAXS1_HEADER_WORDS * 4 ), size * 4 ) ) {
            return NULL;
        }

        temp = data;
        temp += size * 4 + CLIMAXS1_HEADER_WORDS * 4;
        imax = size;
        for ( i = 1; i < imax; i++ ) {
            if ( !RwStreamReadInt32( stream, &size, 4 ) || !RwStreamReadInt32( stream, &type, 4 ) ) {
                return NULL;
            }
            ( (int*)temp )[0] = size;
            ( (int*)temp )[1] = type;
            temp += CLIMAXS1_ARRAY_HEADER;
            switch ( type ) {
            case CLIMAXS1_ARRAY_INT:
                if ( size > 0 && !RwStreamReadInt32( stream, (int*)temp, size * 4 ) ) {
                    return NULL;
                }
                temp += size * 4;
                break;
            case CLIMAXS1_ARRAY_FLOAT:
                if ( size > 0 && !RwStreamReadReal( stream, (float*)temp, size * 4 ) ) {
                    return NULL;
                }
                temp += size * 4;
                break;
            }
        }
    }
    return data;
}

/*
====================
ClimaxS1DataDestroy

Frees a data block.
====================
*/
// 0x801D74F8
void ClimaxS1DataDestroy( char* data ) {
    RwOsGetMemoryInterface()->rwfree( data );
}

/*
====================
ClimaxS1DataGetNumArrays

The number of arrays of a data block.
====================
*/
// no copy in retail (inlined)
inline int ClimaxS1DataGetNumArrays( char* data ) {
    return *(int*)data;
}

/*
====================
ClimaxS1DataGetArrayType

The element type of an array (CLIMAXS1_ARRAY_*).
====================
*/
// no copy in retail (inlined)
inline int ClimaxS1DataGetArrayType( char* data, int array ) {
    int offset = ( (int*)data )[CLIMAXS1_HEADER_WORDS + array];
    return *(int*)( data + offset + 4 );
}

/*
====================
ClimaxS1DataGetSize

The size of a data block in bytes, as it is stored: the arrays and their headers, and the count word.

Not matched: two rows, the operand order of the two adds that accumulate size (retail: the array's
byte count first, then size); wall class register/schedule. Other spellings of the accumulation
(`size = ... * 4 + size + 8`, `size = size + ...`, header first, `sizeof( int ) *` before or after, the
explicit expression) compile to the same order. Also ruled out: the accessors in Gormiti's Debug form (a
char* offset local) and ClimaxS1DataGetNumArrays/GetArrayType defined after this function as plain
functions (Gormiti's order); every compiler version (flagsweep). Gormiti's Debug code (-O0) is reproduced
by this spelling (statement order included), so the difference arises in the optimiser's operand ordering.
Declaring size `unsigned int` makes the function exact, but Gormiti's DWARF (Debug and Final) records
`int size`, so that state is not taken. Also ruled out: an unsigned right-hand side (`sizeof` for the
element or the header, either side), the header added first, a const read of the array count, the
array-type accessor as a char* offset with an index.
====================
*/
// 0x801D7534
int ClimaxS1DataGetSize( char* data ) {
    int i;
    int imax;
    int size;

    imax = ClimaxS1DataGetNumArrays( data );
    size = 0;
    for ( i = 0; i < imax; i++ ) {
        switch ( ClimaxS1DataGetArrayType( data, i ) ) {
        case CLIMAXS1_ARRAY_INT:
            size += ClimaxS1DataGetArraySize( data, i ) * 4 + CLIMAXS1_ARRAY_HEADER;
            break;
        case CLIMAXS1_ARRAY_FLOAT:
            size += ClimaxS1DataGetArraySize( data, i ) * 4 + CLIMAXS1_ARRAY_HEADER;
            break;
        }
    }
    return size + 4;
}

/*
====================
ClimaxS1DataGetArraySize

The number of elements of an array.
====================
*/
// 0x801D75A0
int ClimaxS1DataGetArraySize( char* data, int array ) {
    int offset = ( (int*)data )[CLIMAXS1_HEADER_WORDS + array];
    return *(int*)( data + offset );
}

/*
====================
ClimaxS1DataGetArrayI

The elements of an integer array.
====================
*/
// 0x801D75B4
int* ClimaxS1DataGetArrayI( char* data, int array ) {
    int offset = ( (int*)data )[CLIMAXS1_HEADER_WORDS + array];
    return (int*)( data + offset + CLIMAXS1_ARRAY_HEADER );
}

/*
====================
ClimaxS1DataGetArrayF

The elements of a float array.
====================
*/
// 0x801D75CC
float* ClimaxS1DataGetArrayF( char* data, int array ) {
    int offset = ( (int*)data )[CLIMAXS1_HEADER_WORDS + array];
    return (float*)( data + offset + CLIMAXS1_ARRAY_HEADER );
}

/*
====================
ClimaxS1DataGetValueI

The first element of an integer array.
====================
*/
// 0x801D75E4
int ClimaxS1DataGetValueI( char* data, int attribute ) {
    int offset = ( (int*)data )[CLIMAXS1_HEADER_WORDS + attribute];
    return *(int*)( data + offset + CLIMAXS1_ARRAY_HEADER );
}

/*
====================
ClimaxS1DataAssignAttributeF

Points a float attribute at the values of an array. A NULL data block leaves it unchanged.
====================
*/
// 0x801D75FC
void ClimaxS1DataAssignAttributeF( char* data, int attribute, ClimaxS1AttributeF* attr ) {
    if ( data ) {
        attr->data = ClimaxS1DataGetArrayF( data, attribute );
        attr->size = ClimaxS1DataGetArraySize( data, attribute );
    }
}
