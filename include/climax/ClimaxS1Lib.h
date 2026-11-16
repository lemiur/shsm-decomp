#ifndef CLIMAX_CLIMAXS1LIB_H
#define CLIMAX_CLIMAXS1LIB_H

// Names and layout: Gormiti link map and DWARF (ClimaxS1Lib.obj).

#include <rwsdk/plcore/babinary.h>

/*
====================
ClimaxS1 data block

A block of arrays as it is streamed (the Gormiti functions take it as a char*). It starts with a header
of CLIMAXS1_HEADER_WORDS integers (the number of arrays, then the block's type), followed by one integer
per array: the byte offset of the array from the start of the block. An array is its element count and
its element type (CLIMAXS1_ARRAY_*), then the elements, four byte integers or floats.
====================
*/
#define CLIMAXS1_HEADER_WORDS 2     // invented name
#define CLIMAXS1_ARRAY_HEADER 8     // the count and the type of an array; invented name

// The element types of an array (names invented).
enum {
    CLIMAXS1_ARRAY_INT = 0,
    CLIMAXS1_ARRAY_FLOAT = 1
};

/*
====================
ClimaxS1AttributeF

A float attribute of a data block as the particle code reads it: its values and their count.
====================
*/
class ClimaxS1AttributeF {
public:
    float*  data;
    int     size;
};

extern "C" {
char*   ClimaxS1DataStreamRead( RwStream* stream );
void    ClimaxS1DataDestroy( char* data );
int     ClimaxS1DataGetSize( char* data );
int     ClimaxS1DataGetArraySize( char* data, int array );
int*    ClimaxS1DataGetArrayI( char* data, int array );
float*  ClimaxS1DataGetArrayF( char* data, int array );
int     ClimaxS1DataGetValueI( char* data, int attribute );
void    ClimaxS1DataAssignAttributeF( char* data, int attribute, ClimaxS1AttributeF* attr );
}

#endif
