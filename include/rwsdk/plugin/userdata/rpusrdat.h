// Reference: bfbbdecomp/bfbb include/rwsdk/rpusrdat.h
#ifndef RWSDK_PLUGIN_USERDATA_RPUSRDAT_H
#define RWSDK_PLUGIN_USERDATA_RPUSRDAT_H

#include <rwsdk/baframe.h>
#include <rwsdk/world/bageomet.h>
#include <rwsdk/world/bamateri.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The type of the elements of a user data array. */
typedef enum RpUserDataFormat {
    rpNAUSERDATAFORMAT = 0,
    rpINTUSERDATA,     /* RwInt32 elements */
    rpREALUSERDATA,    /* RwReal elements */
    rpSTRINGUSERDATA,  /* RwChar* elements, each a string of its own */
    rpUSERDATAFORCEENUMSIZEINT = 0x7FFFFFFF
} RpUserDataFormat;

/* RpUserDataArray: a named array of user data elements, all of one format. */
typedef struct RpUserDataArray {
    RwChar* name;               /* 0x0 */
    RpUserDataFormat format;    /* 0x4 */
    RwInt32 numElements;        /* 0x8 */
    void* data;                 /* 0xC */
} RpUserDataArray;

/* RpUserDataList: the user data arrays one object carries (its plugin data). Name invented. */
typedef struct RpUserDataList {
    RwInt32 numElements;        /* 0x0 */
    RpUserDataArray* userData;  /* 0x4 */
} RpUserDataList;

RwInt32 RpGeometryGetUserDataArrayCount(const RpGeometry* geometry);
RpUserDataArray* RpGeometryGetUserDataArray(const RpGeometry* geometry, RwInt32 data);
RwInt32 RwFrameGetUserDataArrayCount(const RwFrame* frame);
RpUserDataArray* RwFrameGetUserDataArray(const RwFrame* frame, RwInt32 data);
RwInt32 RpMaterialGetUserDataArrayCount(const RpMaterial* material);
RpUserDataArray* RpMaterialGetUserDataArray(const RpMaterial* material, RwInt32 data);
RwChar* RpUserDataArrayGetName(RpUserDataArray* userData);
RpUserDataFormat RpUserDataArrayGetFormat(RpUserDataArray* userData);
RwInt32 RpUserDataArrayGetInt(RpUserDataArray* userData, RwInt32 element);
RwChar* RpUserDataArrayGetString(RpUserDataArray* userData, RwInt32 element);
RwBool RpUserDataPluginAttach(void);

#ifdef __cplusplus
}
#endif

#endif
