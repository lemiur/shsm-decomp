// Reference: tgsm/pbg include/rwsdk/rwtypes.h
#ifndef RWSDK_RWTYPES_H
#define RWSDK_RWTYPES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RenderWare 3.7 basic types (rwplcore.h, GameCube/Wii build). */
typedef int             RwBool;
typedef float           RwReal;
typedef char            RwChar;
typedef signed char     RwInt8;
typedef unsigned char   RwUInt8;
typedef short           RwInt16;
typedef unsigned short  RwUInt16;
typedef int             RwInt32;
typedef unsigned int    RwUInt32;

/*
 * Structure alignment. The macro names and their use on the RwMatrix typedef are the SDK's;
 * its Windows build expands RWALIGN to nothing and sets both alignments to sizeof(RwUInt32). The
 * GameCube expansion is in no allowed source: retail lowers the switches of the bamatrix functions
 * that use an RwMatrix as compare trees, which MWCC does only when the function references a type
 * that carries an alignment attribute (docs/exceptions.md, "RwMatrix alignment").
 */
#define RWALIGN(type, x)    type __attribute__((aligned(x)))
#define rwMATRIXALIGNMENT   sizeof(RwUInt32)
#define rwFRAMEALIGNMENT    sizeof(RwUInt32)

#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

/* A chunk or plugin ID: the vendor in the upper 24 bits, the vendor's own ID in the low 8. */
#define MAKECHUNKID(vendorID, chunkID) (((vendorID & 0xFFFFFF) << 8) | (chunkID & 0xFF))

/* Vendor IDs. */
enum RwPluginVendor {
    rwVENDORID_CORE = 0x000000L,
    rwVENDORID_CRITERIONTK = 0x000001L,  /* Criterion's toolkits and plugins (bfbbdecomp/bfbb rwplcore.h) */
    rwVENDORID_CRITERIONINT = 0x000004L,
    rwVENDORID_CRITERIONWORLD = 0x000005L,     /* bfbbdecomp/bfbb rwplcore.h */
    rwPLUGINVENDORFORCEENUMSIZEINT = 0x7FFFFFFF
};

/* The Criterion plugin groups; the core's errors carry rwID_COREPLUGIN. */
enum RwCriterionPluginID {
    rwID_COREPLUGIN = 0x01,
    rwID_WORLDPLUGIN = 0x02,
    rwID_TOOLPLUGIN = 0x03,
    rwID_TOOL2PLUGIN = 0x04,
    rwCRITERIONPLUGINIDFORCEENUMSIZEINT = 0x7FFFFFFF
};
typedef enum RwCriterionPluginID RwCriterionPluginID;

/*
 * IDs of the core's own modules (the low 16 bits of a memory hint name the module); the values are
 * the IDs retail registers the modules with (CorePluginAttach). Enum and member names as in the SDK.
 */
enum RwCriterionCoreID {
    rwID_VECTORMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x01),
    rwID_MATRIXMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x02),
    rwID_FRAMEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x03),
    rwID_STREAMMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x04),
    rwID_CAMERAMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x05),
    rwID_IMAGEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x06),
    rwID_RASTERMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x07),
    rwID_TEXTUREMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x08),
    rwID_PIPEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x09),
    rwID_IMMEDIATEMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x0A),
    rwID_RESOURCESMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x0B),
    rwID_COLORMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x0D),
    rwID_ERRORMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x0F),
    rwID_CHUNKGROUPMODULE = MAKECHUNKID(rwVENDORID_CRITERIONINT, 0x12),
    rwCRITERIONCOREIDFORCEENUMSIZEINT = 0x7FFFFFFF
};

#ifdef __cplusplus
}
#endif

#endif
