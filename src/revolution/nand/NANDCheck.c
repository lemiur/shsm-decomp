// Reference: zeldaret/tp libs/revolution/src/nand/NANDCheck.c
#include <revolution/nand.h>
#include <revolution/nand/NANDCheck.h>
#include <revolution/nand/NANDCore.h>
#include <revolution/fs.h>
#include <revolution/os.h>

/* NANDCommandBlock.state values of the asynchronous space checks (names invented, as are the macros below). */
#define NAND_STATE_CHECK 20
#define NAND_STATE_FREE_SPACE 21

/* Space limits of an application: its home directory, and the user area as a whole. */
#define NAND_HOME_MAX_BLOCKS 0x400
#define NAND_HOME_MAX_INODES 0x21
#define NAND_USER_MAX_BLOCKS 0x4400
#define NAND_USER_MAX_INODES 0xFA0

/* Value the usage counters hold until ISFS has filled them in. */
#define NAND_USAGE_UNKNOWN ((u32)-1)

static const char* USER_DIR_LIST[] = {
    "/meta",
    "/ticket",
    "/title/00010000",
    "/title/00010001",
    "/title/00010003",
    "/title/00010004",
    "/title/00010005",
    "/title/00010006",
    "/title/00010007",
    "/shared2/title",
    NULL,
};

static void nandUserAreaCallback(ISFSError result, void* ctxt);

/*
 * nandCalcUsage
 *
 * Adds up the blocks and inodes that ISFS reports for every directory of the list; a
 * directory that does not exist counts as empty.
 */
s32 nandCalcUsage(u32* fsBlock, u32* inode, const char* dir_list[]) {
    ISFSError ret = ISFS_ERROR_UNKNOWN;
    *fsBlock = 0;
    *inode = 0;

    while (*dir_list) {
        u32 blk = 0;
        u32 node = 0;
        ret = ISFS_GetUsage((const u8*)*dir_list, &blk, &node);

        if (ret == ISFS_ERROR_OK) {
            *fsBlock += blk;
            *inode += node;
        } else if (ret == ISFS_ERROR_NOEXISTS) {
            ret = ISFS_ERROR_OK;
        } else {
            break;
        }

        ++dir_list;
    }

    return ret;
}

/*
 * nandCalcUserUsage
 *
 * nandCalcUsage over the whole user area (USER_DIR_LIST).
 */
static ISFSError nandCalcUserUsage(u32* fsBlock, u32* inode) {
    return nandCalcUsage(fsBlock, inode, USER_DIR_LIST);
}

/*
 * nandFree
 *
 * What is left of a limit after used has been taken from it, never below zero.
 */
static u32 nandFree(u32 limit, u32 used) {
    return (used > limit) ? 0 : limit - used;
}

/*
 * nandCheck
 *
 * Bit mask of the limits that a request of reqBlock blocks and reqInode inodes would exceed:
 * 1 home blocks, 2 home inodes, 4 user blocks, 8 user inodes.
 */
static u32 nandCheck(const u32 reqBlock, const u32 reqInode, const u32 homeBlock, const u32 homeInode, const u32 userBlock, const u32 userInode) {
    u32 answer = 0;

    if (homeBlock + reqBlock > NAND_HOME_MAX_BLOCKS) {
        answer |= 1;
    }

    if (homeInode + reqInode > NAND_HOME_MAX_INODES) {
        answer |= 2;
    }

    if (userBlock + reqBlock > NAND_USER_MAX_BLOCKS) {
        answer |= 4;
    }

    if (userInode + reqInode > NAND_USER_MAX_INODES) {
        answer |= 8;
    }

    return answer;
}

/*
 * NANDCheckAsync
 *
 * Starts the usage count of the home directory; nandUserAreaCallback then walks USER_DIR_LIST
 * and delivers the nandCheck answer mask.
 * Name invented after NANDCheck (not in the reference).
 */
s32 NANDCheckAsync(u32 fsBlock, u32 inode, u32* answer, NANDCallback cb, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    block->reqBlocks = fsBlock;
    block->reqInodes = inode;
    block->answer = answer;
    block->userBlocks = 0;
    block->userInodes = 0;
    block->workBlocks = 0;
    block->workInodes = 0;
    block->dir = USER_DIR_LIST;
    block->state = NAND_STATE_CHECK;

    return nandConvertErrorCode(ISFS_GetUsageAsync((const u8*)nandGetHomeDir(), &block->homeBlocks, &block->homeInodes, nandUserAreaCallback, block));
}

/*
 * nandUserAreaCallback
 *
 * ISFS completion of NANDCheckAsync: folds each directory's usage into the user totals and starts
 * the next one; after the last it stores the result for the state and calls the user callback.
 */
static void nandUserAreaCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;

    if (result == ISFS_ERROR_OK || result == ISFS_ERROR_NOEXISTS) {
        const char* dir = *b->dir;

        if (result == ISFS_ERROR_OK) {
            b->userBlocks += b->workBlocks;
            b->userInodes += b->workInodes;
        }

        if (dir) {
            ISFSError ret;

            ++b->dir;
            ret = ISFS_GetUsageAsync((const u8*)dir, &b->workBlocks, &b->workInodes, nandUserAreaCallback, ctxt);

            if (ret != ISFS_ERROR_OK) {
                b->callback(nandConvertErrorCode(ret), b);
            }
        } else if (b->state == NAND_STATE_CHECK) {
            *b->answer = nandCheck(b->reqBlocks, b->reqInodes, b->homeBlocks, b->homeInodes, b->userBlocks, b->userInodes);
            b->callback(nandConvertErrorCode(ISFS_ERROR_OK), b);
        } else if (b->state == NAND_STATE_FREE_SPACE) {
            u32 userBlocks = nandFree(NAND_USER_MAX_BLOCKS, b->userBlocks);
            u32 userInodes = nandFree(NAND_USER_MAX_INODES, b->userInodes);
            u32 homeBlocks = nandFree(NAND_HOME_MAX_BLOCKS, b->homeBlocks);
            u32 homeInodes = nandFree(NAND_HOME_MAX_INODES, b->homeInodes);

            *(u32*)b->bytes = (userBlocks < homeBlocks) ? userBlocks : homeBlocks;
            *(u32*)b->inodes = (userInodes < homeInodes) ? userInodes : homeInodes;
            b->callback(nandConvertErrorCode(ISFS_ERROR_OK), b);
        } else {
            OSReport("Illegal status is detected at %s()", __FUNCTION__);
            b->callback(nandConvertErrorCode(ISFS_ERROR_UNKNOWN), b);
        }
    } else {
        b->callback(nandConvertErrorCode(result), b);
    }
}

/*
 * NANDGetFreeSpace
 *
 * Blocks and inodes still free in the tighter of the home directory and the user area.
 * Name invented: no reference or string names this function.
 */
s32 NANDGetFreeSpace(u32* freeBlocks, u32* freeInodes) {
    ISFSError ret;
    u32 userBlocks = NAND_USAGE_UNKNOWN;
    u32 userInodes = NAND_USAGE_UNKNOWN;
    u32 homeBlocks = NAND_USAGE_UNKNOWN;
    u32 homeInodes = NAND_USAGE_UNKNOWN;
    u32 userFreeBlocks;
    u32 userFreeInodes;
    u32 homeFreeBlocks;
    u32 homeFreeInodes;

    ret = ISFS_GetUsage((const u8*)nandGetHomeDir(), &homeBlocks, &homeInodes);

    if (ret != ISFS_ERROR_OK) {
        return nandConvertErrorCode(ret);
    }

    /* the home directory's own inode is not counted against the limit */
    homeInodes--;
    ret = nandCalcUserUsage(&userBlocks, &userInodes);

    if (ret != ISFS_ERROR_OK) {
        return nandConvertErrorCode(ret);
    }

    userFreeBlocks = nandFree(NAND_USER_MAX_BLOCKS, userBlocks);
    userFreeInodes = nandFree(NAND_USER_MAX_INODES, userInodes);
    homeFreeBlocks = nandFree(NAND_HOME_MAX_BLOCKS, homeBlocks);
    homeFreeInodes = nandFree(NAND_HOME_MAX_INODES - 1, homeInodes);

    *freeBlocks = (userFreeBlocks < homeFreeBlocks) ? userFreeBlocks : homeFreeBlocks;
    *freeInodes = (userFreeInodes < homeFreeInodes) ? userFreeInodes : homeFreeInodes;

    return NAND_RESULT_OK;
}
