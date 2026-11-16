// Unit name and boundary: Gormiti link map (RPCollis.a ctdata.obj; its source there is plugin/collis/ctdata.c)
// Names, parameters, locals and statement order: Gormiti DWARF (RPCollis.a ctdata.obj), Final and Debug builds.
/*
 * ctdata.c: the collision tree's data: a header, the splits and the map in one block, created, destroyed and
 * streamed. A tree streams as a collision tree chunk holding a struct chunk: the header, the splits, the map.
 * The format before library version 0x36001 (a bounding-box-less list of splits with packed types) is read by
 * _rpCollTreeStreamReadPre36001 and converted.
 */
#include <rwsdk/badevice.h>
#include <rwsdk/babbox.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/rpcriter.h>
#include <rwsdk/plugin/collis/ctdata.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_COLLISPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/*
 * The library linked here was built from an SDK build whose RWBUILDNUMBER was 0x55 (the core's is
 * 0x65), so the chunks this unit writes carry that stamp.
 */
#undef RWBUILDNUMBER
#define RWBUILDNUMBER 0x55

/* The tree as it lies in a stream, little endian. */
typedef struct {
    RwUInt32 flags;         /* 0x00 */
    RwV3d inf;              /* 0x04 */
    RwV3d sup;              /* 0x10 */
    RwUInt32 numEntries;    /* 0x1C */
    RwUInt32 numSplits;     /* 0x20 */
} rpStreamCollTree;

/*
 * ==================== _rpCollTreeCreate ====================
 * Allocates a tree with room for numSplits splits and, with the map flag, numEntries map entries behind
 * its header. Returns NULL, with the error set, when the block cannot be allocated. 0x802B8494
 */
RpCollTree *_rpCollTreeCreate(RwUInt16 numEntries, RwUInt16 numSplits, RwBBox *bbox, RwUInt32 flags)
{
    RwUInt32 size;
    RpCollTree *tree;
    RwUInt8 *mem;

    size = sizeof(RpCollTree);
    if (numSplits) {
        size += numSplits * sizeof(RpCollSplit) + 15;
    }
    if (flags & rpCOLLTREE_USEMAP) {
        size += numEntries * sizeof(RwUInt16) + 1;
    }

    mem = (RwUInt8 *)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_COLLTREE);
    if (mem == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    tree = (RpCollTree *)mem;
    tree->flags = flags;
    tree->numEntries = numEntries;
    tree->numSplits = numSplits;
    tree->bbox = *bbox;

    mem += sizeof(RpCollTree);
    if (numSplits) {
        mem = (RwUInt8 *)(((RwUInt32)mem + 15) & ~15);
        tree->splits = (RpCollSplit *)mem;
        mem += numSplits * sizeof(RpCollSplit);
    } else {
        tree->splits = NULL;
    }
    if (flags & rpCOLLTREE_USEMAP) {
        mem = (RwUInt8 *)(((RwUInt32)mem + 1) & ~1);
        tree->map = (RwUInt16 *)mem;
    } else {
        tree->map = NULL;
    }
    return tree;
}

/*
 * ==================== _rpCollTreeDestroy ====================
 * Frees the tree's block. 0x802B85D8
 */
void _rpCollTreeDestroy(RpCollTree *tree)
{
    RwFree(tree);
}

/*
 * ==================== _rpCollTreeStreamGetSize ====================
 * The bytes the tree takes in a stream: the struct chunk's header, the stream header, the splits and the map. 0x802B85E8
 */
RwUInt32 _rpCollTreeStreamGetSize(const RpCollTree *tree)
{
    RwUInt32 size;

    size = rwCHUNKHEADERSIZE + sizeof(rpStreamCollTree) + tree->numSplits * sizeof(RpCollSplit);
    if (tree->flags & rpCOLLTREE_USEMAP) {
        size += tree->numEntries * sizeof(RwUInt16);
    }
    return size;
}

/*
 * ==================== _rpCollTreeStreamWrite ====================
 * Writes the tree as a collision tree chunk holding a struct chunk. Returns the tree, or NULL on a failed write. 0x802B8614
 */
const RpCollTree *_rpCollTreeStreamWrite(const RpCollTree *tree, RwStream *stream)
{
    RwUInt32 size;
    rpStreamCollTree streamCollTree;

    size = _rpCollTreeStreamGetSize(tree);

    if (!RwStreamWriteChunkHeader(stream, rwID_COLLTREE, size)) {
        return NULL;
    }
    if (!RwStreamWriteChunkHeader(stream, rwID_STRUCT, size - rwCHUNKHEADERSIZE)) {
        return NULL;
    }

    streamCollTree.inf = tree->bbox.inf;
    streamCollTree.sup = tree->bbox.sup;
    streamCollTree.flags = tree->flags;
    streamCollTree.numEntries = tree->numEntries;
    streamCollTree.numSplits = tree->numSplits;
    RwMemLittleEndian32(&streamCollTree, sizeof(streamCollTree));
    if (!RwStreamWrite(stream, &streamCollTree, sizeof(streamCollTree))) {
        return NULL;
    }

    if (tree->numSplits) {
        RwUInt32 i;

        for (i = 0; i < tree->numSplits; i++) {
            RpCollSplit temp;

            temp = tree->splits[i];
            RwMemLittleEndian16(&temp.leftSector.index, sizeof(RwUInt16));
            RwMemLittleEndian16(&temp.rightSector.index, sizeof(RwUInt16));
            RwMemLittleEndian32(&temp.leftSector.value, sizeof(RwReal));
            RwMemLittleEndian32(&temp.rightSector.value, sizeof(RwReal));
            if (!RwStreamWrite(stream, &temp, sizeof(temp))) {
                return NULL;
            }
        }
    }

    if (tree->flags & rpCOLLTREE_USEMAP) {
        if (!RwStreamWriteInt16(stream, (const RwInt16 *)tree->map, tree->numEntries * sizeof(RwUInt16))) {
            return NULL;
        }
    }
    return tree;
}

/*
 * ==================== _rpCollTreeStreamRead ====================
 * Reads a tree from the collision tree chunk the stream is in front of. Returns the tree, or NULL on a failed read. 0x802B881C
 */
RpCollTree *_rpCollTreeStreamRead(RwStream *stream)
{
    RpCollTree *tree;
    RwUInt32 version;
    rpStreamCollTree streamCollTree;
    RwBBox bbox;
    RwUInt32 size;

    if (!RwStreamFindChunk(stream, rwID_STRUCT, NULL, &version)) {
        return NULL;
    }
    if (RwStreamRead(stream, &streamCollTree, sizeof(streamCollTree)) != sizeof(streamCollTree)) {
        return NULL;
    }
    RwMemNative32(&streamCollTree, sizeof(streamCollTree));

    bbox.inf = streamCollTree.inf;
    bbox.sup = streamCollTree.sup;
    tree = _rpCollTreeCreate(streamCollTree.numEntries, streamCollTree.numSplits, &bbox, streamCollTree.flags);
    if (tree == NULL) {
        return NULL;
    }

    if (tree->numSplits) {
        RwUInt32 i;
        RpCollSplit *split;

        size = tree->numSplits * sizeof(RpCollSplit);
        if (RwStreamRead(stream, tree->splits, size) != size) {
            RwFree(tree);
            return NULL;
        }
        split = tree->splits;
        for (i = 0; i < tree->numSplits; i++, split++) {
            RwMemNative16(&split->leftSector.index, sizeof(RwUInt16));
            RwMemNative16(&split->rightSector.index, sizeof(RwUInt16));
            RwMemNative32(&split->leftSector.value, sizeof(RwReal));
            RwMemNative32(&split->rightSector.value, sizeof(RwReal));
        }
    }

    if (tree->flags & rpCOLLTREE_USEMAP) {
        if (!RwStreamReadInt16(stream, (RwInt16 *)tree->map, tree->numEntries * sizeof(RwUInt16))) {
            RwFree(tree);
            return NULL;
        }
    }
    return tree;
}

/*
 * ==================== _rpCollTreeStreamReadPre36001 ====================
 * Reads a tree of the format before 0x36001: the splits as two packed words (the sides' types and indices) and
 * two reals, then the leaves' counts and first entries in tree order. The leaves are found by walking the
 * splits with a stack. A leaf count above rpCOLLSECTOR_CONTENTS_MAXCOUNT cannot be converted: failConv is set.
 * Returns the tree, or NULL on a failed read or conversion. numLeafNodes counts the leaves for the Debug build's
 * assertions only. The side types are stored through the split (the left side's read back from the right's), and
 * leftType and rightType hold the packed contents codes, as in Gormiti's Debug build; that statement form gives
 * retail's register colouring. 0x802B89E4
 */
RpCollTree *_rpCollTreeStreamReadPre36001(RwStream *stream, RwUInt32 numEntries, RwUInt32 numSplits,
                                          RwBBox *bbox, RwBool *failConv)
{
    RpCollTree *tree;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(stream);
    RWASSERT((numEntries > 0) && (numEntries < 0x10000));
    RWASSERT(numSplits < 0x10000);
    RWASSERT(bbox);
    RWASSERT(failConv);

    *failConv = FALSE;

    tree = _rpCollTreeCreate(numEntries, numSplits, bbox, rpCOLLTREE_USEMAP);
    if (tree == NULL) {
        return NULL;
    }

    if (tree->numSplits) {
        RpCollSplit *split;
        RwUInt32 i, numLeafNodes;

        split = tree->splits;
        for (i = 0; i < tree->numSplits; i++, split++) {
            RwUInt32 types, indices, leftType, rightType;

            if (!RwStreamReadInt32(stream, (RwInt32 *)&types, sizeof(RwUInt32))
                || !RwStreamReadInt32(stream, (RwInt32 *)&indices, sizeof(RwUInt32))
                || !RwStreamReadReal(stream, &split->leftSector.value, sizeof(RwReal))
                || !RwStreamReadReal(stream, &split->rightSector.value, sizeof(RwReal))) {
                RwFree(tree);
                return NULL;
            }

            split->rightSector.type = (RwUInt8)(types >> 16);
            split->leftSector.type = split->rightSector.type | rpCOLLSECTOR_TYPE_NEG;
            split->rightSector.index = indices & 0xFFFF;
            split->leftSector.index = indices >> 16;

            leftType = (types >> 8) & 0xFF;
            if (leftType == 2) {
                split->leftSector.contents = rpCOLLSECTOR_CONTENTS_SPLIT;
            } else {
                split->leftSector.contents = 0;
            }

            rightType = types & 0xFF;
            if (rightType == 2) {
                split->rightSector.contents = rpCOLLSECTOR_CONTENTS_SPLIT;
            } else {
                split->rightSector.contents = 0;
            }
        }

        {
            RpCollSector start;
            RpCollSector *curr, *stack[rpCOLLTREE_MAXDEPTH + 1];
            RwUInt32 nStack;

            start.contents = rpCOLLSECTOR_CONTENTS_SPLIT;
            start.index = 0;
            curr = &start;
            nStack = 1;
            numLeafNodes = 0;
            while (nStack != 0) {
                if (curr->contents != rpCOLLSECTOR_CONTENTS_SPLIT) {
                    RwUInt16 count;
                    RwUInt32 data;

                    /* Debug build: assertion at this point (Gormiti Debug) */
                    RWASSERT(curr->index == numLeafNodes);

                    if (!RwStreamReadInt32(stream, (RwInt32 *)&data, sizeof(RwUInt32))) {
                        RwFree(tree);
                        return NULL;
                    }
                    count = data >> 16;
                    if (count > rpCOLLSECTOR_CONTENTS_MAXCOUNT) {
                        *failConv = TRUE;
                        RwFree(tree);
                        RWERROR((E_RP_COLLIS_BUILD_FAILURE));
                        return NULL;
                    }
                    curr->contents = count;
                    curr->index = data;
                    numLeafNodes++;
                    curr = stack[--nStack];
                } else {
                    stack[nStack++] = &tree->splits[curr->index].rightSector;
                    curr = &tree->splits[curr->index].leftSector;
                }
            }

            /* Debug build: assertion at this point (Gormiti Debug) */
            RWASSERT(numLeafNodes == tree->numSplits + 1);
        }
    } else {
        if (!RwStreamSkip(stream, sizeof(RwUInt32))) {
            RwFree(tree);
            return NULL;
        }
    }

    if (tree->flags & rpCOLLTREE_USEMAP) {
        RwUInt32 i;

        for (i = 0; i < tree->numEntries; i++) {
            RwInt32 index;

            if (!RwStreamReadInt32(stream, &index, sizeof(RwInt32))) {
                RwFree(tree);
                return NULL;
            }
            tree->map[i] = index;
        }
    }
    return tree;
}
