// Reference: bfbbdecomp/bfbb config/GQPE78/symbols.txt and splits.txt (function names, unit layout)
/*
 * rpusrdat.c: the user data plugin. Geometries, world sectors, frames, cameras, lights, materials
 * and textures each carry a list of named user data arrays (integers, reals or strings), copied
 * with their object and streamed in its extension chunk. RpGeometryAddUserDataArray and the
 * other Add/Remove/Set functions, RpUserDataGetFormatSize and the accessors of the other object
 * types have no copy in retail (dead-stripped). The list helpers (UserDataListCopy and the ones
 * named after it) and UserDataArrayDestroy have no copy either: each is inlined into its callers.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/babincam.h>
#include <rwsdk/babinfrm.h>
#include <rwsdk/babintex.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plugin/userdata/rpusrdat.h>
#include <rwsdk/world/balight.h>
#include <rwsdk/world/basector.h>
#include <string.h>

/* The user data plugin's registration with the engine (only its instances are counted). */
static RwModuleInfo userDataModule;

/* Offsets of the user data list in each object type, and of its stream chunk. Names invented. */
static RwInt32 userDataTextureStreamOffset;
static RwInt32 userDataTextureOffset;
static RwInt32 userDataMaterialStreamOffset;
static RwInt32 userDataMaterialOffset;
static RwInt32 userDataLightStreamOffset;
static RwInt32 userDataLightOffset;
static RwInt32 userDataCameraStreamOffset;
static RwInt32 userDataCameraOffset;
static RwInt32 userDataFrameStreamOffset;
static RwInt32 userDataFrameOffset;
static RwInt32 userDataWorldSectorStreamOffset;
static RwInt32 userDataWorldSectorOffset;
static RwInt32 userDataGeometryStreamOffset;
static RwInt32 userDataGeometryOffset;

/* Copies a string into a new allocation (NULL for none). Name invented. */
#define rwstrdup(_dst, _src)                                                                  \
    do {                                                                                      \
        (_dst) = NULL;                                                                        \
        if ((_src) != NULL) {                                                                 \
            (_dst) = (RwChar*)RwMalloc(rwstrlen(_src) + 1, rwMEMHINTDUR_EVENT | rwID_STRING); \
            if ((_dst) != NULL) {                                                             \
                rwstrcpy((_dst), (_src));                                                     \
            }                                                                                 \
        }                                                                                     \
    } while (0)

/* The size in bytes of one element of the format. Name invented (RpUserDataGetFormatSize's body). */
static RwInt32 UserDataFormatSize(RpUserDataFormat format) {
    switch (format) {
    case rpINTUSERDATA:
        return sizeof(RwInt32);
    case rpREALUSERDATA:
        return sizeof(RwReal);
    case rpSTRINGUSERDATA:
        return sizeof(RwChar*);
    default:
        return 0;
    }
}

/* Counts an engine instance using the plugin. */
static void* UserDataOpen(void* instance, RwInt32 offset, RwInt32 size) {
    userDataModule.numInstances++;

    return instance;
} // 0x802CAFF0

/* Uncounts an engine instance using the plugin. */
static void* UserDataClose(void* instance, RwInt32 offset, RwInt32 size) {
    userDataModule.numInstances--;

    return instance;
} // 0x802CB004

/*
 * Copies one user data array: its name and its elements (strings each into a copy of its own).
 * Name invented.
 */
static void UserDataArrayCopy(RpUserDataArray* dst, RpUserDataArray* src) {
    RwInt32 dataSize;
    RwChar** dstStrings;
    RwChar** srcStrings;
    RwInt32 i;

    dst->format = src->format;
    dst->numElements = src->numElements;

    if (src->name != NULL) {
        rwstrdup(dst->name, src->name);
    }

    if (src->data != NULL) {
        dataSize = dst->numElements * UserDataFormatSize(dst->format);
        dst->data = RwMalloc(dataSize, rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);

        if (dst->format == rpSTRINGUSERDATA) {
            dstStrings = (RwChar**)dst->data;
            srcStrings = (RwChar**)src->data;
            for (i = 0; i < dst->numElements; i++) {
                if (srcStrings[i] == NULL) {
                    dstStrings[i] = NULL;
                } else {
                    rwstrdup(dstStrings[i], srcStrings[i]);
                }
            }
        } else {
            memcpy(dst->data, src->data, dataSize);
        }
    }
} // 0x802CB018

/* Reads one user data array: its name, format, element count and elements. */
static RwStream* UserDataStreamRead(RpUserDataArray* userData, RwStream* stream) {
    RwInt32 length;
    RwChar** strings;
    RwInt32 i;

    if (!RwStreamReadInt32(stream, &length, sizeof(RwInt32))) {
        return NULL;
    }
    if (length > 0) {
        userData->name = (RwChar*)RwMalloc(length, rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        if (userData->name == NULL) {
            return NULL;
        }
        if (!RwStreamRead(stream, userData->name, length)) {
            return NULL;
        }
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)&userData->format, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamReadInt32(stream, &userData->numElements, sizeof(RwInt32))) {
        return NULL;
    }

    switch (userData->format) {
    case rpINTUSERDATA:
        userData->data = RwMalloc(userData->numElements * sizeof(RwInt32),
                                  rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        if (userData->data == NULL) {
            return NULL;
        }
        if (!RwStreamReadInt32(stream, (RwInt32*)userData->data,
                               userData->numElements * sizeof(RwInt32))) {
            return NULL;
        }
        break;
    case rpREALUSERDATA:
        userData->data = RwMalloc(userData->numElements * sizeof(RwReal),
                                  rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        if (userData->data == NULL) {
            return NULL;
        }
        if (!RwStreamReadReal(stream, (RwReal*)userData->data,
                              userData->numElements * sizeof(RwReal))) {
            return NULL;
        }
        break;
    case rpSTRINGUSERDATA:
        userData->data = RwMalloc(userData->numElements * sizeof(RwChar*),
                                  rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        if (userData->data == NULL) {
            return NULL;
        }
        strings = (RwChar**)userData->data;
        for (i = 0; i < userData->numElements; i++) {
            if (!RwStreamReadInt32(stream, &length, sizeof(RwInt32))) {
                return NULL;
            }
            if (length > 0) {
                strings[i] = (RwChar*)RwMalloc(length, rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
                if (strings[i] == NULL) {
                    return NULL;
                }
                if (!RwStreamRead(stream, strings[i], length)) {
                    return NULL;
                }
            } else {
                strings[i] = NULL;
            }
        }
        break;
    default:
        return NULL;
    }

    return stream;
} // 0x802CB1E8

/* Writes one user data array (as UserDataStreamRead reads it); strings carry their terminator. */
static RwStream* UserDataStreamWrite(RpUserDataArray* userData, RwStream* stream) {
    RwInt32 length;
    RwChar** strings;
    RwInt32 i;

    if (userData->name != NULL) {
        length = rwstrlen(userData->name) + 1;
    } else {
        length = 0;
    }
    if (!RwStreamWriteInt32(stream, &length, sizeof(RwInt32))) {
        return NULL;
    }
    if (length > 0 && !RwStreamWrite(stream, userData->name, length)) {
        return NULL;
    }

    if (!RwStreamWriteInt32(stream, (const RwInt32*)&userData->format, sizeof(RwInt32))) {
        return NULL;
    }
    if (!RwStreamWriteInt32(stream, &userData->numElements, sizeof(RwInt32))) {
        return NULL;
    }

    switch (userData->format) {
    case rpINTUSERDATA:
        if (!RwStreamWriteInt32(stream, (const RwInt32*)userData->data,
                                userData->numElements * sizeof(RwInt32))) {
            return NULL;
        }
        break;
    case rpREALUSERDATA:
        if (!RwStreamWriteReal(stream, (const RwReal*)userData->data,
                               userData->numElements * sizeof(RwReal))) {
            return NULL;
        }
        break;
    case rpSTRINGUSERDATA:
        strings = (RwChar**)userData->data;
        for (i = 0; i < userData->numElements; i++) {
            if (strings[i] != NULL) {
                length = rwstrlen(strings[i]) + 1;
            } else {
                length = 0;
            }
            if (!RwStreamWriteInt32(stream, &length, sizeof(RwInt32))) {
                return NULL;
            }
            if (length > 0 && !RwStreamWrite(stream, strings[i], length)) {
                return NULL;
            }
        }
        break;
    default:
        return NULL;
    }

    return stream;
} // 0x802CB47C

/* The size of one user data array in a stream. Name invented. */
static RwInt32 UserDataStreamGetSize(const RpUserDataArray* userData) {
    RwChar** strings;
    RwInt32 i;
    RwInt32 size = 0;
    RwInt32 length;

    if (userData != NULL) {
        size = sizeof(RwInt32);
        if (userData->name != NULL) {
            length = rwstrlen(userData->name) + 1;
            size += length;
        }
        size += sizeof(RwInt32) + sizeof(RwInt32);

        switch (userData->format) {
        case rpINTUSERDATA:
            size += userData->numElements * sizeof(RwInt32);
            break;
        case rpREALUSERDATA:
            size += userData->numElements * sizeof(RwReal);
            break;
        case rpSTRINGUSERDATA:
            strings = (RwChar**)userData->data;
            for (i = 0; i < userData->numElements; i++) {
                size += sizeof(RwInt32);
                if (strings[i] != NULL) {
                    length = rwstrlen(strings[i]) + 1;
                    size += length;
                }
            }
            break;
        }
    }

    return size;
} // 0x802CB684

/* Writes the count of a list's formatted arrays, then those arrays. Name invented. */
static RwStream* UserDataListStreamWrite(const RpUserDataList* list, RwStream* stream) {
    RwInt32 numArrays;
    RwInt32 i;

    if (list != NULL && list->numElements > 0) {
        numArrays = 0;
        for (i = 0; i < list->numElements; i++) {
            if (list->userData[i].format != rpNAUSERDATAFORMAT) {
                numArrays++;
            }
        }

        if (numArrays != 0) {
            if (!RwStreamWriteInt32(stream, &numArrays, sizeof(RwInt32))) {
                return NULL;
            }
            for (i = 0; i < list->numElements; i++) {
                if (list->userData[i].format != rpNAUSERDATAFORMAT) {
                    stream = UserDataStreamWrite(&list->userData[i], stream);
                }
            }
        }

        return stream;
    }

    return NULL;
} // 0x802CB784

/* Gives a new object an empty user data list. */
static void* UserDataObjectConstruct(void* object, RwInt32 offset, RwInt32 size) {
    RpUserDataList* list = RWPLUGINOFFSET(RpUserDataList, object, offset);

    list->numElements = 0;
    list->userData = NULL;

    return object;
} // 0x802CB894

/* Frees one user data array's name, strings and elements. Name invented. */
static void UserDataArrayDestroy(RpUserDataArray* userData) {
    RwChar** strings;
    RwInt32 i;

    if (userData->name != NULL) {
        RwFree(userData->name);
        userData->name = NULL;
    }

    if (userData->format == rpSTRINGUSERDATA) {
        strings = (RwChar**)userData->data;
        for (i = 0; i < userData->numElements; i++) {
            if (strings[i] != NULL) {
                RwFree(strings[i]);
            }
        }
    }

    if (userData->data != NULL) {
        RwFree(userData->data);
        userData->data = NULL;
    }
}

/* Frees a list's arrays and empties it. Name invented. */
static void UserDataListDestroy(RpUserDataList* list) {
    RwInt32 i;

    if (list->userData != NULL) {
        for (i = 0; i < list->numElements; i++) {
            UserDataArrayDestroy(&list->userData[i]);
        }

        RwFree(list->userData);
    }

    list->userData = NULL;
    list->numElements = 0;
}

/* Replaces a list's arrays with copies of another list's. */
static void UserDataListCopy(RpUserDataList* dst, const RpUserDataList* src) {
    RwInt32 i;

    UserDataListDestroy(dst);

    dst->numElements = src->numElements;
    if (dst->numElements > 0) {
        dst->userData = (RpUserDataArray*)RwMalloc(dst->numElements * sizeof(RpUserDataArray),
                                                   rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        for (i = 0; i < dst->numElements; i++) {
            UserDataArrayCopy(&dst->userData[i], &src->userData[i]);
        }
    }
}

/* Frees an object's user data arrays and empties its list. */
static void* UserDataObjectDestruct(void* object, RwInt32 offset, RwInt32 size) {
    RpUserDataList* list = RWPLUGINOFFSET(RpUserDataList, object, offset);

    UserDataListDestroy(list);

    return object;
} // 0x802CB8A4

/* Replaces the destination object's user data with a copy of the source object's. */
static void* UserDataObjectCopy(void* dstObject, const void* srcObject, RwInt32 offset, RwInt32 size) {
    RpUserDataList* dstList = RWPLUGINOFFSET(RpUserDataList, dstObject, offset);
    const RpUserDataList* srcList = RWPLUGINOFFSET(const RpUserDataList, srcObject, offset);

    UserDataListCopy(dstList, srcList);

    return dstObject;
} // 0x802CB9B8

/* Reads a list's arrays after their count. Name invented. */
static RwStream* UserDataListStreamRead(RpUserDataList* list, RwStream* stream) {
    RwInt32 i;
    RwInt32 numArrays;

    if (list != NULL && RwStreamReadInt32(stream, &numArrays, sizeof(RwInt32))) {
        list->numElements = numArrays;
        list->userData = (RpUserDataArray*)RwMalloc(list->numElements * sizeof(RpUserDataArray),
                                                    rwMEMHINTDUR_EVENT | rwID_USERDATAPLUGIN);
        for (i = 0; i < list->numElements; i++) {
            stream = UserDataStreamRead(&list->userData[i], stream);
        }
    }

    return stream;
}

/* Reads an object's user data arrays from its extension chunk. */
static RwStream* UserDataObjectStreamRead(RwStream* stream, RwInt32 binaryLength, void* object,
                                          RwInt32 offset, RwInt32 size) {
    RpUserDataList* list = RWPLUGINOFFSET(RpUserDataList, object, offset);

    UserDataListStreamRead(list, stream);

    return stream;
} // 0x802CBB30

/* Writes an object's user data arrays into its extension chunk. */
static RwStream* UserDataObjectStreamWrite(RwStream* stream, RwInt32 binaryLength, const void* object,
                                           RwInt32 offset, RwInt32 size) {
    UserDataListStreamWrite(RWPLUGINOFFSET(const RpUserDataList, object, offset), stream);

    return stream;
} // 0x802CBBD8

/* The size of a list's formatted arrays and their count in a stream (none without one). Name invented. */
static RwInt32 UserDataListStreamGetSize(const RpUserDataList* list) {
    RwInt32 i;
    RwInt32 size = 0;

    if (list != NULL && list->numElements > 0) {
        for (i = 0; i < list->numElements; i++) {
            if (list->userData[i].format != rpNAUSERDATAFORMAT) {
                size += UserDataStreamGetSize(&list->userData[i]);
            }
        }
        if (size > 0) {
            size += sizeof(RwInt32);
        }
    }

    return size;
}

/* The size of an object's user data in its extension chunk. */
static RwInt32 UserDataObjectGetSize(const void* object, RwInt32 offset, RwInt32 size) {
    return UserDataListStreamGetSize(RWPLUGINOFFSET(const RpUserDataList, object, offset));
} // 0x802CBC10

/* The number of a list's arrays that hold elements. Name invented. */
static RwInt32 UserDataListGetCount(const RpUserDataList* list) {
    RwInt32 count = 0;
    RwInt32 i;

    for (i = 0; i < list->numElements; i++) {
        if (list->userData[i].data != NULL) {
            count++;
        }
    }

    return count;
}

/* The number of the geometry's user data arrays that hold elements. */
RwInt32 RpGeometryGetUserDataArrayCount(const RpGeometry* geometry) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, geometry, userDataGeometryOffset);

    return UserDataListGetCount(list);
} // 0x802CBCB0

/* The geometry's user data array of the index, or NULL past the last one. */
RpUserDataArray* RpGeometryGetUserDataArray(const RpGeometry* geometry, RwInt32 data) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, geometry, userDataGeometryOffset);

    if (data < list->numElements) {
        return &list->userData[data];
    }

    return NULL;
} // 0x802CBCF8

/* The number of the frame's user data arrays that hold elements. */
RwInt32 RwFrameGetUserDataArrayCount(const RwFrame* frame) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, frame, userDataFrameOffset);

    return UserDataListGetCount(list);
} // 0x802CBD20

/* The frame's user data array of the index, or NULL past the last one. */
RpUserDataArray* RwFrameGetUserDataArray(const RwFrame* frame, RwInt32 data) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, frame, userDataFrameOffset);

    if (data < list->numElements) {
        return &list->userData[data];
    }

    return NULL;
} // 0x802CBD68

/* The number of the material's user data arrays that hold elements. */
RwInt32 RpMaterialGetUserDataArrayCount(const RpMaterial* material) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, material, userDataMaterialOffset);

    return UserDataListGetCount(list);
} // 0x802CBD90

/* The material's user data array of the index, or NULL past the last one. */
RpUserDataArray* RpMaterialGetUserDataArray(const RpMaterial* material, RwInt32 data) {
    const RpUserDataList* list = RWPLUGINOFFSET(const RpUserDataList, material, userDataMaterialOffset);

    if (data < list->numElements) {
        return &list->userData[data];
    }

    return NULL;
} // 0x802CBDD8

/* The array's name. */
RwChar* RpUserDataArrayGetName(RpUserDataArray* userData) {
    return userData->name;
} // 0x802CBE00

/* The format of the array's elements. */
RpUserDataFormat RpUserDataArrayGetFormat(RpUserDataArray* userData) {
    return userData->format;
} // 0x802CBE08

/* An element of an integer array. */
RwInt32 RpUserDataArrayGetInt(RpUserDataArray* userData, RwInt32 element) {
    return ((RwInt32*)userData->data)[element];
} // 0x802CBE10

/* An element of a string array. */
RwChar* RpUserDataArrayGetString(RpUserDataArray* userData, RwInt32 element) {
    return ((RwChar**)userData->data)[element];
} // 0x802CBE20

/*
 * Attaches the user data plugin: registers it with the engine, and its data and stream chunk with
 * each object type. The world sector registrations are checked against the geometry's offsets,
 * not their own (the original's slip, kept).
 */
RwBool RpUserDataPluginAttach(void) {
    if (RwEngineRegisterPlugin(0, rwID_USERDATAPLUGIN, UserDataOpen, UserDataClose) < 0) {
        return FALSE;
    }

    userDataGeometryOffset = RpGeometryRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                      UserDataObjectConstruct, UserDataObjectDestruct,
                                                      UserDataObjectCopy);
    if (userDataGeometryOffset < 0) {
        return FALSE;
    }
    userDataGeometryStreamOffset = RpGeometryRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataGeometryStreamOffset < 0) {
        return FALSE;
    }

    userDataWorldSectorOffset = RpWorldSectorRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                            UserDataObjectConstruct, UserDataObjectDestruct,
                                                            UserDataObjectCopy);
    if (userDataGeometryOffset < 0) {
        return FALSE;
    }
    userDataWorldSectorStreamOffset = RpWorldSectorRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataGeometryStreamOffset < 0) {
        return FALSE;
    }

    userDataFrameOffset = RwFrameRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                UserDataObjectConstruct, UserDataObjectDestruct,
                                                UserDataObjectCopy);
    if (userDataFrameOffset < 0) {
        return FALSE;
    }
    userDataFrameStreamOffset = RwFrameRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataFrameStreamOffset < 0) {
        return FALSE;
    }

    userDataCameraOffset = RwCameraRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                  UserDataObjectConstruct, UserDataObjectDestruct,
                                                  UserDataObjectCopy);
    if (userDataCameraOffset < 0) {
        return FALSE;
    }
    userDataCameraStreamOffset = RwCameraRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataCameraStreamOffset < 0) {
        return FALSE;
    }

    userDataLightOffset = RpLightRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                UserDataObjectConstruct, UserDataObjectDestruct,
                                                UserDataObjectCopy);
    if (userDataLightOffset < 0) {
        return FALSE;
    }
    userDataLightStreamOffset = RpLightRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataLightStreamOffset < 0) {
        return FALSE;
    }

    userDataMaterialOffset = RpMaterialRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                      UserDataObjectConstruct, UserDataObjectDestruct,
                                                      UserDataObjectCopy);
    if (userDataMaterialOffset < 0) {
        return FALSE;
    }
    userDataMaterialStreamOffset = RpMaterialRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataMaterialStreamOffset < 0) {
        return FALSE;
    }

    userDataTextureOffset = RwTextureRegisterPlugin(sizeof(RpUserDataList), rwID_USERDATAPLUGIN,
                                                    UserDataObjectConstruct, UserDataObjectDestruct,
                                                    UserDataObjectCopy);
    if (userDataTextureOffset < 0) {
        return FALSE;
    }
    userDataTextureStreamOffset = RwTextureRegisterPluginStream(
        rwID_USERDATAPLUGIN, UserDataObjectStreamRead, UserDataObjectStreamWrite, UserDataObjectGetSize);
    if (userDataTextureStreamOffset < 0) {
        return FALSE;
    }

    return TRUE;
} // 0x802CBE30
