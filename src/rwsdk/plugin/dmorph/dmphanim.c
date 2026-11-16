// Unit name and boundary: Gormiti link map (RpDMorph.a dmphanim.obj)
// Names, parameters, locals and statement order: Gormiti DWARF (RpDMorph.a dmphanim.obj), Final and Debug builds.
/*
 * dmphanim.c: the delta-morph animation, one sequence of keys per delta-morph target. The sequences sit behind
 * the header in one block, and each sequence owns the block of its keys.
 */
#include <string.h>
#include <rwsdk/badevice.h>
#include <rwsdk/plcore/babinary.h>
#include <rwsdk/plcore/bamemory.h>
#include <rwsdk/plcore/bastream.h>
#include <rwsdk/plugin/dmorph/rpdmorph.h>

/* The plugin's error ID (rpplugin.h in the plugin's own sources). */
#define rwPLUGIN_ID rwID_DMORPHPLUGIN
#include <rwsdk/plcore/rpdbgerr.h>

/* The newest version of the animation's stream that is read (name invented; value from the version test). */
#define rpDMORPHANIMATIONSTREAMVERSION 0x100

/*
 * ==================== RpDMorphAnimationCreate ====================
 * Creates an animation of numDMorphTargets sequences, each without keys. Returns NULL when the engine has no
 * memory for it.
 */
RpDMorphAnimation* RpDMorphAnimationCreate(RwUInt32 numDMorphTargets) {
    RpDMorphAnimation* anim;
    RwUInt32 size;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(numDMorphTargets > 0);

    size = sizeof(RpDMorphAnimation) + numDMorphTargets * sizeof(rpDMorphFrameSequence);
    anim = (RpDMorphAnimation*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_DMORPHANIMATION);
    if (anim == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    anim->numDMorphTargets = numDMorphTargets;
    anim->frameSequences = (rpDMorphFrameSequence*)(anim + 1);

    {
        RwUInt32 i;
        rpDMorphFrameSequence* sequence = anim->frameSequences;

        for (i = 0; i < numDMorphTargets; i++) {
            sequence->numFrames = 0;
            sequence->frames = NULL;
            sequence++;
        }
    }

    return anim;
} // 0x802B9080

/*
 * ==================== RpDMorphAnimationDestroy ====================
 * Frees the keys of every sequence, then the animation. Returns NULL.
 */
RpDMorphAnimation* RpDMorphAnimationDestroy(RpDMorphAnimation* anim) {
    RwUInt32 i;
    rpDMorphFrameSequence* sequence;


    sequence = anim->frameSequences;
    for (i = 0; i < anim->numDMorphTargets; i++) {
        if (sequence->frames) {
            RwFree(sequence->frames);
        }
        sequence++;
    }

    RwFree(anim);

    return NULL;
} // 0x802B91B8

/*
 * ==================== RpDMorphAnimationCreateFrames ====================
 * Gives the sequence of one delta-morph target numFrames keys, each ending its chain (nextFrame -1). Returns NULL
 * when the engine has no memory for them.
 */
RpDMorphAnimation* RpDMorphAnimationCreateFrames(RpDMorphAnimation* anim, RwUInt32 dMorphTargetIndex,
                                                 RwUInt32 numFrames) {
    rpDMorphFrameSequence* sequence;
    RwUInt32 size;
    RwUInt32 i;

    /* Debug build: assertion at this point (Gormiti Debug) */
    RWASSERT(dMorphTargetIndex < anim->numDMorphTargets);
    RWASSERT(numFrames > 0);

    sequence = &anim->frameSequences[dMorphTargetIndex];
    RWASSERT(sequence->frames == NULL);

    size = numFrames * sizeof(rpDMorphFrame);
    sequence->frames = (rpDMorphFrame*)RwMalloc(size, rwMEMHINTDUR_EVENT | rwID_DMORPHPLUGIN);
    if (sequence->frames == NULL) {
        RWERROR((E_RW_NOMEM, size));
        return NULL;
    }

    memset(sequence->frames, 0, size);
    for (i = 0; i < numFrames; i++) {
        sequence->frames[i].nextFrame = -1;
    }
    sequence->numFrames = numFrames;

    return anim;
} // 0x802B9244

/*
 * ==================== RpDMorphAnimationStreamRead ====================
 * Reads an animation from the struct chunk the stream is at: the version, the flags (unused), the number of
 * targets, the total number of keys (unused), then for each target the number of its keys and the keys.
 * Returns NULL when the stream fails or the version is newer than this reader. The Debug build also totals the
 * bytes it reads (checkSize) and asserts that the total is the chunk's size; that bookkeeping is not in retail.
 */
RpDMorphAnimation* RpDMorphAnimationStreamRead(RwStream* stream) {
    RpDMorphAnimation* animation;
    RwUInt32 subChunkSize;
    RwUInt32 libVersion;
    RwUInt32 animVersion;
    RwUInt32 animFlags;
    RwUInt32 numDMorphTargets;
    RwUInt32 totalNumFrames;
    RwUInt32 i;


    if (!RwStreamFindChunk(stream, rwID_STRUCT, &subChunkSize, &libVersion)) {
        return NULL;
    }

    if (!RwStreamReadInt32(stream, (RwInt32*)&animVersion, sizeof(animVersion)) ||
        !RwStreamReadInt32(stream, (RwInt32*)&animFlags, sizeof(animFlags)) ||
        !RwStreamReadInt32(stream, (RwInt32*)&numDMorphTargets, sizeof(numDMorphTargets)) ||
        !RwStreamReadInt32(stream, (RwInt32*)&totalNumFrames, sizeof(totalNumFrames))) {
        return NULL;
    }

    if (animVersion > rpDMORPHANIMATIONSTREAMVERSION) {
        RWERROR((E_RW_BADVERSION));
        return NULL;
    }

    animation = RpDMorphAnimationCreate(numDMorphTargets);
    if (animation == NULL) {
        return NULL;
    }

    for (i = 0; i < numDMorphTargets; i++) {
        RwUInt32 numFrames;

        if (!RwStreamReadInt32(stream, (RwInt32*)&numFrames, sizeof(numFrames))) {
            return NULL;
        }

        if (numFrames) {
            rpDMorphFrame* frame;
            RwUInt32 j;

            if (!RpDMorphAnimationCreateFrames(animation, i, numFrames)) {
                return NULL;
            }

            frame = animation->frameSequences[i].frames;
            RWASSERT(frame != NULL);
            for (j = 0; j < numFrames; j++) {
                if (!RwStreamReadReal(stream, &frame->startValue, 4 * sizeof(RwReal)) ||
                    !RwStreamReadInt32(stream, (RwInt32*)&frame->nextFrame, sizeof(frame->nextFrame))) {
                    return NULL;
                }
                frame++;
            }
        }
    }

    return animation;
} // 0x802B93D0
