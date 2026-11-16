// Reference: tgsm/pbg include/rwsdk/plcore/babinary.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwplcore.h
#ifndef RWSDK_PLCORE_BABINARY_H
#define RWSDK_PLCORE_BABINARY_H

#include <rwsdk/plcore/bastream.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Chunk types of the core (members as bfbbdecomp/bfbb rwplcore.h). */
enum RwCorePluginID {
    rwID_NAOBJECT = MAKECHUNKID(rwVENDORID_CORE, 0x00),
    rwID_STRUCT = MAKECHUNKID(rwVENDORID_CORE, 0x01),
    rwID_STRING = MAKECHUNKID(rwVENDORID_CORE, 0x02),
    rwID_EXTENSION = MAKECHUNKID(rwVENDORID_CORE, 0x03),    /* the plugin data of an object */
    rwID_CAMERA = MAKECHUNKID(rwVENDORID_CORE, 0x05),
    rwID_TEXTURE = MAKECHUNKID(rwVENDORID_CORE, 0x06),
    rwID_MATERIAL = MAKECHUNKID(rwVENDORID_CORE, 0x07),
    rwID_MATLIST = MAKECHUNKID(rwVENDORID_CORE, 0x08),
    rwID_ATOMICSECT = MAKECHUNKID(rwVENDORID_CORE, 0x09),
    rwID_PLANESECT = MAKECHUNKID(rwVENDORID_CORE, 0x0A),
    rwID_WORLD = MAKECHUNKID(rwVENDORID_CORE, 0x0B),
    rwID_SPLINE = MAKECHUNKID(rwVENDORID_CORE, 0x0C),
    rwID_MATRIX = MAKECHUNKID(rwVENDORID_CORE, 0x0D),
    rwID_FRAMELIST = MAKECHUNKID(rwVENDORID_CORE, 0x0E),
    rwID_GEOMETRY = MAKECHUNKID(rwVENDORID_CORE, 0x0F),
    rwID_CLUMP = MAKECHUNKID(rwVENDORID_CORE, 0x10),
    rwID_LIGHT = MAKECHUNKID(rwVENDORID_CORE, 0x12),
    rwID_UNICODESTRING = MAKECHUNKID(rwVENDORID_CORE, 0x13),
    rwID_ATOMIC = MAKECHUNKID(rwVENDORID_CORE, 0x14),
    rwID_TEXTURENATIVE = MAKECHUNKID(rwVENDORID_CORE, 0x15),
    rwID_TEXDICTIONARY = MAKECHUNKID(rwVENDORID_CORE, 0x16),
    rwID_ANIMDATABASE = MAKECHUNKID(rwVENDORID_CORE, 0x17),
    rwID_IMAGE = MAKECHUNKID(rwVENDORID_CORE, 0x18),
    rwID_SKINANIMATION = MAKECHUNKID(rwVENDORID_CORE, 0x19),
    rwID_GEOMETRYLIST = MAKECHUNKID(rwVENDORID_CORE, 0x1A),
    rwID_HANIMANIMATION = MAKECHUNKID(rwVENDORID_CORE, 0x1B),
    rwID_TEAM = MAKECHUNKID(rwVENDORID_CORE, 0x1C),
    rwID_CROWD = MAKECHUNKID(rwVENDORID_CORE, 0x1D),
    rwID_DMORPHANIMATION = MAKECHUNKID(rwVENDORID_CORE, 0x1E),
    rwID_RIGHTTORENDER = MAKECHUNKID(rwVENDORID_CORE, 0x1F),
    rwID_MTEFFECTNATIVE = MAKECHUNKID(rwVENDORID_CORE, 0x20),
    rwID_MTEFFECTDICT = MAKECHUNKID(rwVENDORID_CORE, 0x21),
    rwID_TEAMDICTIONARY = MAKECHUNKID(rwVENDORID_CORE, 0x22),
    rwID_PITEXDICTIONARY = MAKECHUNKID(rwVENDORID_CORE, 0x23),
    rwID_TOC = MAKECHUNKID(rwVENDORID_CORE, 0x24),
    rwID_PRTSTDGLOBALDATA = MAKECHUNKID(rwVENDORID_CORE, 0x25),
    rwID_ALTPIPE = MAKECHUNKID(rwVENDORID_CORE, 0x26),
    rwID_PIPEDS = MAKECHUNKID(rwVENDORID_CORE, 0x27),
    rwID_PATCHMESH = MAKECHUNKID(rwVENDORID_CORE, 0x28),
    rwID_COREPLUGINIDMAX = MAKECHUNKID(rwVENDORID_CORE, 0x29),
    rwCOREPLUGINIDFORCEENUMSIZEINT = 0x7FFFFFFF
};

/* Size of a chunk header in a stream (type, length, library version stamp). */
#define rwCHUNKHEADERSIZE (sizeof(RwUInt32) * 3)

/*
 * The oldest library version whose streams this library reads, and its own version and build
 * (values from retail's version checks and chunk stamps).
 */
#define rwLIBRARYBASEVERSION 0x35000
#define rwLIBRARYCURRENTVERSION 0x37002
#define RWBUILDNUMBER 0x65

/* RwChunkHeaderInfo: a chunk header as RwStreamReadChunkHeaderInfo returns it. */
typedef struct RwChunkHeaderInfo {
    RwUInt32 type;      /* 0x0: chunk ID */
    RwUInt32 length;    /* 0x4: length of the chunk data in bytes */
    RwUInt32 version;   /* 0x8: library version that wrote the chunk */
    RwUInt32 buildNum;  /* 0xC: library build that wrote the chunk */
    RwBool isComplex;   /* 0x10: the chunk holds further chunks */
} RwChunkHeaderInfo;

RwBool _rwStreamReadChunkHeader(RwStream* stream, RwUInt32* type, RwUInt32* length,
                                RwUInt32* version, RwUInt32* buildNum);
RwStream* _rwStreamWriteVersionedChunkHeader(RwStream* stream, RwInt32 type, RwInt32 size,
                                             RwUInt32 version, RwUInt32 buildNum);
RwBool RwStreamFindChunk(RwStream* stream, RwUInt32 type, RwUInt32* lengthOut, RwUInt32* versionOut);
void* RwMemLittleEndian32(void* mem, RwUInt32 size);
void* RwMemLittleEndian16(void* mem, RwUInt32 size);
void* RwMemNative32(void* mem, RwUInt32 size);
void* RwMemNative16(void* mem, RwUInt32 size);
RwStream* RwStreamWriteReal(RwStream* stream, const RwReal* reals, RwUInt32 numBytes);
RwStream* RwStreamWriteInt32(RwStream* stream, const RwInt32* ints, RwUInt32 numBytes);
RwStream* RwStreamWriteInt16(RwStream* stream, const RwInt16* ints, RwUInt32 numBytes);
RwStream* RwStreamReadInt16(RwStream* stream, RwInt16* ints, RwUInt32 numBytes);
RwStream* RwStreamReadReal(RwStream* stream, RwReal* reals, RwUInt32 numBytes);
RwStream* RwStreamReadInt32(RwStream* stream, RwInt32* ints, RwUInt32 numBytes);
RwStream* RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* chunkHeaderInfo);

/* Writes a chunk header stamped with this library's version. */
#define RwStreamWriteChunkHeader(_stream, _type, _size) \
    _rwStreamWriteVersionedChunkHeader((_stream), (_type), (_size), rwLIBRARYCURRENTVERSION, RWBUILDNUMBER)

#ifdef __cplusplus
}
#endif

#endif
