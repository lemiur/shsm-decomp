// Reference: tgsm/pbg src/rwsdk/src/plcore/babinary.c
/*
 * babinary.c: binary stream helpers. Chunk headers (type, length and the version stamp of the
 * library that wrote them) and the byte-order conversion of streamed data: streams are
 * little-endian, so this big-endian platform swaps every word it reads or writes.
 */
#include <string.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/badebug.h>
#include <rwsdk/plcore/baerr.h>

/* A chunk header as it is stored in a stream. Name invented. */
typedef struct rwChunkHeader {
    RwUInt32 type;      /* 0x0: chunk ID */
    RwUInt32 length;    /* 0x4: length of the chunk data in bytes */
    RwUInt32 version;   /* 0x8: packed library version and build */
} rwChunkHeader;

/*
 * A chunk header's version stamp packs the library version (0x3VVRR, e.g. 0x37002 for 3.7.0.2:
 * major 3, minor and patch digits VV, revision RR) and the build: bits 22-31 hold VV
 * ((version - 0x30000) >> 8), bits 16-21 RR (version & 0x3F), bits 0-15 the build number. Old
 * libraries stored version >> 8 alone, leaving the upper half zero.
 */

/* Words the conversion buffer of RwStreamWriteReal and RwStreamWriteInt32 holds. Name invented. */
#define rwSTREAMCONVERTBUFFERSIZE 64

/* Whether chunks of type info->type hold further chunks. */
static RwBool ChunkIsComplex(RwChunkHeaderInfo* info) {
    switch (info->type) {
        case rwID_STRUCT:
            return FALSE;
        case rwID_STRING:
            return FALSE;
        case rwID_EXTENSION:
            return FALSE;
        case rwID_CAMERA:
            return TRUE;
        case rwID_TEXTURE:
            return TRUE;
        case rwID_MATERIAL:
            return TRUE;
        case rwID_MATLIST:
            return TRUE;
        case rwID_ATOMICSECT:
            return TRUE;
        case rwID_PLANESECT:
            return TRUE;
        case rwID_WORLD:
            return TRUE;
        case rwID_MATRIX:
            return FALSE;
        case rwID_FRAMELIST:
            return TRUE;
        case rwID_GEOMETRY:
            return TRUE;
        case rwID_CLUMP:
            return TRUE;
        case rwID_LIGHT:
            return TRUE;
        case rwID_UNICODESTRING:
            return FALSE;
        case rwID_ATOMIC:
            return TRUE;
        case rwID_GEOMETRYLIST:
            return TRUE;
        default:
            return FALSE;
    }
} // 0x8029FF00

/*
 * Reads a chunk header. A version stamp with nothing in its upper half is an old library's
 * version, stored shifted right by 8 and without a build number.
 */
RwBool _rwStreamReadChunkHeader(RwStream* stream, RwUInt32* type, RwUInt32* length,
                                RwUInt32* version, RwUInt32* buildNum) {
    rwChunkHeader header;
    RwChunkHeaderInfo info;
    RwBool success;

    success = RwStreamRead(stream, &header, sizeof(header)) == sizeof(header);
    if (!success) {
        RWERROR((E_RW_READ));
        return FALSE;
    }

    RwMemNative32(&header, sizeof(header));

    info.type = header.type;
    info.length = header.length;

    if ((header.version & 0xFFFF0000) == 0) {
        info.version = header.version << 8;
        info.buildNum = 0;
    } else {
        info.version = (((header.version >> 14) & 0x3FF00) + 0x30000) | ((header.version >> 16) & 0x3F);
        info.buildNum = header.version & 0xFFFF;
    }

    info.isComplex = ChunkIsComplex(&info);

    if (type != NULL) {
        *type = info.type;
    }
    if (length != NULL) {
        *length = info.length;
    }
    if (buildNum != NULL) {
        *buildNum = info.buildNum;
    }
    if (version != NULL) {
        *version = info.version;
    }

    return TRUE;
} // 0x8029FFBC

/* Writes a chunk header stamped with version and buildNum. */
RwStream* _rwStreamWriteVersionedChunkHeader(RwStream* stream, RwInt32 type, RwInt32 size,
                                             RwUInt32 version, RwUInt32 buildNum) {
    rwChunkHeader header;

    header.type = type;
    header.length = size;
    header.version = (((version - 0x30000) << 14) & 0xFFC00000) | ((version & 0x3F) << 16) | (buildNum & 0xFFFF);

    RwMemLittleEndian32(&header, sizeof(header));

    return RwStreamWrite(stream, &header, sizeof(header));
} // 0x802A013C

/*
 * Skips chunks up to the next chunk of type type. Fails at the end of the stream or when that
 * chunk was written by a library version this one cannot read.
 */
RwBool RwStreamFindChunk(RwStream* stream, RwUInt32 type, RwUInt32* lengthOut, RwUInt32* versionOut) {
    RwUInt32 readType;
    RwUInt32 readLength;
    RwUInt32 readVersion;

    while (_rwStreamReadChunkHeader(stream, &readType, &readLength, &readVersion, NULL)) {
        if (readType == type) {
            if (readVersion < rwLIBRARYBASEVERSION) {
                RWERROR((E_RW_BADVERSION));
                return FALSE;
            }
            if (readVersion > rwLIBRARYCURRENTVERSION) {
                RWERROR((E_RW_BADVERSION));
                return FALSE;
            }

            if (lengthOut != NULL) {
                *lengthOut = readLength;
            }
            if (versionOut != NULL) {
                *versionOut = readVersion;
            }

            return TRUE;
        }

        if (!RwStreamSkip(stream, readLength)) {
            return FALSE;
        }
    }

    return FALSE;
} // 0x802A01BC

/* Converts size bytes of 32-bit words at mem from native to little-endian order. */
void* RwMemLittleEndian32(void* mem, RwUInt32 size) {
    RwUInt32* word = (RwUInt32*)mem;

    size >>= 2;
    while (size--) {
        /* reverse the four bytes */
        *word = ((*word & 0xFF000000) >> 24) | ((*word & 0x00FF0000) >> 8) |
                ((*word & 0x0000FF00) << 8) | ((*word & 0x000000FF) << 24);
        word++;
    }

    return mem;
} // 0x802A02F8

/* Converts size bytes of 16-bit words at mem from native to little-endian order. */
void* RwMemLittleEndian16(void* mem, RwUInt32 size) {
    RwUInt16* word = (RwUInt16*)mem;

    size >>= 1;
    while (size--) {
        *word = (RwUInt16)((*word >> 8) | (*word << 8));
        word++;
    }

    return mem;
} // 0x802A03BC

/* Converts size bytes of 32-bit words at mem from little-endian to native order. */
void* RwMemNative32(void* mem, RwUInt32 size) {
    RwUInt32* word = (RwUInt32*)mem;

    size >>= 2;
    while (size--) {
        /* reverse the four bytes */
        *word = ((*word & 0xFF000000) >> 24) | ((*word & 0x00FF0000) >> 8) |
                ((*word & 0x0000FF00) << 8) | ((*word & 0x000000FF) << 24);
        word++;
    }

    return mem;
} // 0x802A0484

/* Converts size bytes of 16-bit words at mem from little-endian to native order. */
void* RwMemNative16(void* mem, RwUInt32 size) {
    RwUInt16* word = (RwUInt16*)mem;

    size >>= 1;
    while (size--) {
        *word = (RwUInt16)((*word >> 8) | (*word << 8));
        word++;
    }

    return mem;
} // 0x802A0548

/*
 * Writes numBytes of reals in little-endian order. RwReal is already a 32-bit IEEE float on this
 * platform, so the reals need no conversion beyond the byte order (retail's body is
 * RwStreamWriteInt32's).
 */
RwStream* RwStreamWriteReal(RwStream* stream, const RwReal* reals, RwUInt32 numBytes) {
    RwReal buffer[rwSTREAMCONVERTBUFFERSIZE];
    RwUInt32 bytesToWrite;

    while (numBytes != 0) {
        bytesToWrite = sizeof(buffer);
        if (numBytes < sizeof(buffer)) {
            bytesToWrite = numBytes;
        }

        memcpy(buffer, reals, bytesToWrite);
        RwMemLittleEndian32(buffer, bytesToWrite);

        if (!RwStreamWrite(stream, buffer, bytesToWrite)) {
            return NULL;
        }

        numBytes -= bytesToWrite;
        reals = (const RwReal*)((const RwUInt8*)reals + bytesToWrite);
    }

    return stream;
} // 0x802A0610

/* Writes numBytes of 32-bit integers in little-endian order. */
RwStream* RwStreamWriteInt32(RwStream* stream, const RwInt32* ints, RwUInt32 numBytes) {
    RwInt32 buffer[rwSTREAMCONVERTBUFFERSIZE];
    RwUInt32 bytesToWrite;

    while (numBytes != 0) {
        bytesToWrite = sizeof(buffer);
        if (numBytes < sizeof(buffer)) {
            bytesToWrite = numBytes;
        }

        memcpy(buffer, ints, bytesToWrite);
        RwMemLittleEndian32(buffer, bytesToWrite);

        if (!RwStreamWrite(stream, buffer, bytesToWrite)) {
            return NULL;
        }

        numBytes -= bytesToWrite;
        ints = (const RwInt32*)((const RwUInt8*)ints + bytesToWrite);
    }

    return stream;
} // 0x802A0770

/* Writes numBytes of 16-bit integers in little-endian order. */
RwStream* RwStreamWriteInt16(RwStream* stream, const RwInt16* ints, RwUInt32 numBytes) {
    RwInt16 buffer[rwSTREAMCONVERTBUFFERSIZE * 2];
    RwUInt32 bytesToWrite;

    while (numBytes != 0) {
        bytesToWrite = sizeof(buffer);
        if (numBytes < sizeof(buffer)) {
            bytesToWrite = numBytes;
        }

        memcpy(buffer, ints, bytesToWrite);
        RwMemLittleEndian16(buffer, bytesToWrite);

        if (!RwStreamWrite(stream, buffer, bytesToWrite)) {
            return NULL;
        }

        numBytes -= bytesToWrite;
        ints = (const RwInt16*)((const RwUInt8*)ints + bytesToWrite);
    }

    return stream;
} // 0x802A08D0

/* Reads numBytes of little-endian 16-bit integers. */
RwStream* RwStreamReadInt16(RwStream* stream, RwInt16* ints, RwUInt32 numBytes) {
    if (!RwStreamRead(stream, ints, numBytes)) {
        RWERROR((E_RW_READ));
        return NULL;
    }

    RwMemNative16(ints, numBytes);

    return stream;
} // 0x802A0A34

/* Reads numBytes of little-endian reals (see RwStreamWriteReal). */
RwStream* RwStreamReadReal(RwStream* stream, RwReal* reals, RwUInt32 numBytes) {
    if (!RwStreamRead(stream, reals, numBytes)) {
        RWERROR((E_RW_READ));
        return NULL;
    }

    RwMemNative32(reals, numBytes);

    return stream;
} // 0x802A0B70

/* Reads numBytes of little-endian 32-bit integers. */
RwStream* RwStreamReadInt32(RwStream* stream, RwInt32* ints, RwUInt32 numBytes) {
    if (!RwStreamRead(stream, ints, numBytes)) {
        RWERROR((E_RW_READ));
        return NULL;
    }

    RwMemNative32(ints, numBytes);

    return stream;
} // 0x802A0CA8

/* Reads a chunk header into chunkHeaderInfo. */
RwStream* RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* chunkHeaderInfo) {
    RwUInt32 type;
    RwUInt32 length;
    RwUInt32 version;
    RwUInt32 buildNum;

    if (!_rwStreamReadChunkHeader(stream, &type, &length, &version, &buildNum)) {
        return NULL;
    }

    chunkHeaderInfo->type = type;
    chunkHeaderInfo->length = length;
    chunkHeaderInfo->version = version;
    chunkHeaderInfo->buildNum = buildNum;
    chunkHeaderInfo->isComplex = ChunkIsComplex(chunkHeaderInfo);

    return stream;
} // 0x802A0DE0
