/* Unit name unconfirmed: the malloc pool allocator, MSL alloc.c by recall; the filename is a guess. */
// Reference: zeldaret/ss src/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Src/alloc.c (macro forms of SubBlock_size, SubBlock_set_free, __msize_inline)
// Reference: zeldaret/tp libs/PowerPC_EABI_Support/MSL/MSL_C/MSL_Common/Src/alloc.c (__mem_pool handle, __pool_free, get_malloc_pool)
#include <stdlib.h>
#include <string.h>
#include <MSL_C/MSL_Common/alloc.h>

/*
 * Pool allocator behind malloc, calloc, realloc and free.
 *
 * Requests of up to 68 bytes come from fixed-size pools, one pool per client
 * size (fix_pool_sizes); everything larger is carved out of variable-size
 * blocks. Both kinds of block are taken from the system heap with __sys_alloc.
 *
 * A variable-size Block starts with a 16 byte header, followed by SubBlocks.
 * Its last 8 bytes are a sentinel word and the head of the ring of free
 * SubBlocks (Block_start). The low three bits of every size word are flags:
 * SUBBLOCK_IN_USE is set while the SubBlock is allocated and
 * SUBBLOCK_PREV_IN_USE while the SubBlock in front of it is. A free SubBlock
 * repeats its size in its last word, which is how the SubBlock behind it finds
 * it. The low bit of SubBlock::block_ is always set; that is what tells a
 * variable-size SubBlock from a FixSubBlock, whose block_ is an aligned
 * pointer.
 *
 * The structure and function names follow the Metrowerks MSL alloc.c as
 * remembered; none of them is confirmed by this binary.
 */

typedef struct Block {
    struct Block* prev_;
    struct Block* next_;
    unsigned long max_size_;
    unsigned long size_;
} Block;

typedef struct SubBlock {
    unsigned long size_;
    Block* block_;
    struct SubBlock* prev_;
    struct SubBlock* next_;
} SubBlock;

typedef struct FixSubBlock {
    struct FixBlock* block_;
    struct FixSubBlock* next_;
} FixSubBlock;

typedef struct FixBlock {
    struct FixBlock* prev_;
    struct FixBlock* next_;
    unsigned long client_size_;
    FixSubBlock* start_;
    unsigned long n_allocated_;
} FixBlock;

typedef struct FixStart {
    FixBlock* tail_;
    FixBlock* head_;
} FixStart;

#define NUM_FIX_POOLS 6

typedef struct __mem_pool_obj {
    Block* start_;
    FixStart fix_start[NUM_FIX_POOLS];
} __mem_pool_obj;

/* The opaque handle the pool entry points take; it is the storage of one __mem_pool_obj. */
typedef struct __mem_pool {
    void* reserved[13];
} __mem_pool;

/* Flags in the low bits of a size word. */
#define SUBBLOCK_IN_USE 2
#define SUBBLOCK_PREV_IN_USE 4
#define SIZE_FLAGS 7
#define SIZE_MASK 0xFFFFFFF8

#define SUBBLOCK_OVERHEAD 8 /* size_ and block_ in front of the client area */
#define FIXSUBBLOCK_OVERHEAD 4 /* block_ in front of the client area */
#define BLOCK_OVERHEAD 24 /* 16 byte Block header and the 8 byte sentinel at its end */
#define MIN_SUBBLOCK_SIZE 80 /* smallest SubBlock that is handed out or left behind by a split */
#define NEW_BLOCK_MIN_SIZE 0x10000 /* a new Block is never smaller than this */

#define MAX_FIX_CLIENT_SIZE 68 /* largest request served from the fixed pools */
#define MAX_POOL_REQUEST 0xFFFFFFCF /* larger requests would overflow once the headers are added */
#define MAX_RESIZE_REQUEST 0xFFFFFF7F /* resizing clamps its request to this */
#define FIX_BLOCK_DATA_SIZE 4076 /* data size a new FixBlock aims for */
#define FIX_BLOCK_MAX_CLIENTS 256
#define FIX_BLOCK_MIN_CLIENTS 10 /* a FixBlock is not worth making for fewer clients */

static const unsigned long fix_pool_sizes[NUM_FIX_POOLS] = {4, 12, 20, 36, 52, 68};

static __mem_pool protopool;
static unsigned char initted = 0;

/* Size of a SubBlock without its flag bits. */
#define SubBlock_size(ths) ((ths)->size_ & SIZE_MASK)

#define SubBlock_is_free(ths) (!((ths)->size_ & SUBBLOCK_IN_USE))
#define SubBlock_is_prev_alloc(ths) ((ths)->size_ & SUBBLOCK_PREV_IN_USE)
#define SubBlock_block(ths) ((Block*)((unsigned long)(ths)->block_ & ~1))
#define SubBlock_set_block(ths, bp) ((ths)->block_ = (Block*)((unsigned long)(bp) | 1))
#define SubBlock_neighbor(ths, sz) ((SubBlock*)((char*)(ths) + (sz)))
#define SubBlock_next_neighbor(ths) SubBlock_neighbor(ths, SubBlock_size(ths))
/* byteptr: allocator header, the size word repeated in the last word of a free SubBlock (zeldaret/ss alloc.c SubBlock_footer) */
#define SubBlock_footer(ths, sz) (*(unsigned long*)((char*)(ths) + (sz)-4))
#define SubBlock_from_pointer(ptr) ((SubBlock*)((char*)(ptr) - SUBBLOCK_OVERHEAD))
#define SubBlock_client(ths) ((void*)((char*)(ths) + SUBBLOCK_OVERHEAD))
#define FixSubBlock_from_pointer(ptr) ((FixSubBlock*)((char*)(ptr) - FIXSUBBLOCK_OVERHEAD))
#define FixSubBlock_client(ths) ((void*)((char*)(ths) + FIXSUBBLOCK_OVERHEAD))

/*
 * Block_size
 *
 * Size of a Block without its flag bits.
 */
static inline unsigned long Block_size(Block* ths) {
    return ths->size_ & SIZE_MASK;
}
/* byteptr: allocator header, the first-free-SubBlock pointer in the last word of a Block (zeldaret/ss alloc.c Block_start) */
#define Block_start(ths) (*(SubBlock**)((char*)(ths) + Block_size(ths) - 4))

/*
 * Block_empty
 *
 * True when a Block holds a single free SubBlock covering all of it.
 */
static inline int Block_empty(Block* ths) {
    SubBlock* first = (SubBlock*)((char*)ths + sizeof(Block));

    return SubBlock_is_free(first) && SubBlock_size(first) == Block_size(ths) - BLOCK_OVERHEAD;
}

/*
 * SubBlock_set_size
 *
 * Gives a SubBlock a new size, keeping its flags, and repeats the size in the
 * footer when the SubBlock is free.
 */
#define SubBlock_set_size(ths, sz) \
    do { \
        (ths)->size_ &= SIZE_FLAGS; \
        (ths)->size_ |= (sz) & SIZE_MASK; \
        if (SubBlock_is_free(ths)) { \
            SubBlock_footer(ths, sz) = (sz); \
        } \
    } while (0)

/*
 * SubBlock_set_free
 *
 * Marks a SubBlock free: clears its in-use flag, tells the SubBlock behind it
 * and writes the size footer.
 */
// byteptr: allocator header, the next SubBlock's size word and this SubBlock's footer (zeldaret/ss alloc.c SubBlock_set_free)
#define SubBlock_set_free(ths) \
    unsigned long this_size = SubBlock_size(ths); \
    (ths)->size_ &= ~SUBBLOCK_IN_USE; \
    *(unsigned long*)((char*)(ths) + this_size) &= ~SUBBLOCK_PREV_IN_USE; \
    SubBlock_footer(ths, this_size) = this_size

/*
 * SubBlock_set_not_free
 *
 * Marks a SubBlock allocated and tells the SubBlock behind it.
 */
static inline void SubBlock_set_not_free(SubBlock* ths) {
    SubBlock* np = SubBlock_next_neighbor(ths);

    ths->size_ |= SUBBLOCK_IN_USE;
    np->size_ |= SUBBLOCK_PREV_IN_USE;
}

/*
 * SubBlock_construct
 *
 * Initialises a SubBlock of sz bytes inside bp.
 */
static inline void SubBlock_construct(SubBlock* ths, unsigned long sz, Block* bp, int prev_alloc, int this_alloc) {
    SubBlock_set_block(ths, bp);
    ths->size_ = sz;

    if (prev_alloc) {
        ths->size_ |= SUBBLOCK_PREV_IN_USE;
    }

    if (this_alloc) {
        ths->size_ |= SUBBLOCK_IN_USE;
        SubBlock_neighbor(ths, sz)->size_ |= SUBBLOCK_PREV_IN_USE;
    } else {
        SubBlock_footer(ths, sz) = sz;
    }
}

/*
 * SubBlock_merge_prev
 *
 * Merges a free SubBlock into a free neighbour in front of it, if there is one.
 * Returns the SubBlock that now holds the memory.
 */
static inline SubBlock* SubBlock_merge_prev(SubBlock* ths, SubBlock** start) {
    SubBlock* p;
    unsigned long prevsz;

    if (!SubBlock_is_prev_alloc(ths)) {
        prevsz = SubBlock_footer(ths, 0);

        if (prevsz & SUBBLOCK_IN_USE) {
            return ths;
        }

        p = (SubBlock*)((char*)ths - prevsz);
        SubBlock_set_size(p, prevsz + SubBlock_size(ths));

        if (*start == ths) {
            *start = (*start)->next_;
        }

        ths->next_->prev_ = ths->prev_;
        ths->next_->prev_->next_ = ths->next_;
        return p;
    }

    return ths;
}

static void SubBlock_merge_next(SubBlock* ths, SubBlock** start);

/*
 * SubBlock_split
 *
 * Cuts a SubBlock after sz bytes. The remainder becomes a SubBlock of its own,
 * linked behind the first one when that was free.
 */
static inline void SubBlock_split(SubBlock* ths, unsigned long sz) {
    int isfree = SubBlock_is_free(ths);
    int prev_alloc = SubBlock_is_prev_alloc(ths);
    unsigned long oldsz = SubBlock_size(ths);
    Block* bp = SubBlock_block(ths);
    SubBlock* np = (SubBlock*)((char*)ths + sz);

    SubBlock_construct(ths, sz, bp, prev_alloc, !isfree);
    SubBlock_construct(np, oldsz - sz, bp, !isfree, !isfree);

    if (isfree) {
        np->next_ = ths->next_;
        np->next_->prev_ = np;
        np->prev_ = ths;
        ths->next_ = np;
    }
}

/*
 * Block_subBlock
 *
 * Finds a free SubBlock of at least sz bytes in a Block, splits off the excess when
 * it is worth keeping, and takes it out of the free ring. Without a fit it records
 * the largest free size in the Block and returns 0.
 */
static SubBlock* Block_subBlock(Block* ths, unsigned long sz, unsigned long* max_found) {
    SubBlock* st = Block_start(ths);
    SubBlock* b;
    SubBlock** start;
    unsigned long bsz;
    unsigned long max_size;

    if (st == 0) {
        ths->max_size_ = 0;
        return 0;
    }

    b = st;
    bsz = SubBlock_size(b);
    max_size = bsz;

    while (bsz < sz) {
        b = b->next_;
        bsz = SubBlock_size(b);

        if (max_size < bsz) {
            max_size = bsz;
        }

        if (b == st) {
            ths->max_size_ = max_size;

            if (max_found) {
                *max_found = max_size - SUBBLOCK_OVERHEAD;
            }

            return 0;
        }
    }

    if (bsz - sz >= MIN_SUBBLOCK_SIZE) {
        SubBlock_split(b, sz);
    }

    Block_start(ths) = b->next_;
    SubBlock_set_not_free(b);
    start = &Block_start(ths);

    if (*start == b) {
        *start = b->next_;
    }

    if (*start == b) {
        *start = 0;
        ths->max_size_ = 0;
    } else {
        b->next_->prev_ = b->prev_;
        b->prev_->next_ = b->next_;
    }

    if (max_found) {
        *max_found = SubBlock_size(b) - SUBBLOCK_OVERHEAD;
    }

    return b;
} // 0x80195D28

/*
 * Block_link
 *
 * Puts a SubBlock on the free ring of its Block and merges it with its neighbours.
 */
static void Block_link(Block* ths, SubBlock* sb) {
    SubBlock** st;

    SubBlock_set_free(sb);
    st = &Block_start(ths);

    if (*st) {
        sb->prev_ = (*st)->prev_;
        sb->prev_->next_ = sb;
        sb->next_ = *st;
        (*st)->prev_ = sb;
        *st = sb;
        *st = SubBlock_merge_prev(*st, st);
        SubBlock_merge_next(*st, st);
    } else {
        sb->next_ = sb->prev_ = *st = sb;
    }

    if (ths->max_size_ < SubBlock_size(*st)) {
        ths->max_size_ = SubBlock_size(*st);
    }

} // 0x80195F2C

/*
 * SubBlock_merge_next
 *
 * Merges a free SubBlock with a free neighbour behind it, if there is one.
 */
static void SubBlock_merge_next(SubBlock* ths, SubBlock** start) {
    SubBlock* np = SubBlock_next_neighbor(ths);
    unsigned long this_size;

    if (!SubBlock_is_free(np)) {
        return;
    }

    this_size = SubBlock_size(ths) + SubBlock_size(np);
    SubBlock_set_size(ths, this_size);

    if (SubBlock_is_free(ths)) {
        SubBlock_neighbor(ths, this_size)->size_ &= ~SUBBLOCK_PREV_IN_USE;
    } else {
        SubBlock_neighbor(ths, this_size)->size_ |= SUBBLOCK_PREV_IN_USE;
    }

    if (*start == np) {
        *start = (*start)->next_;
    }

    if (*start == np) {
        *start = 0;
    }

    np->next_->prev_ = np->prev_;
    np->prev_->next_ = np->next_;
} // 0x8019607C

/*
 * link_new_block
 *
 * Takes a new Block from the system heap and links it into the pool.
 */
static inline Block* link_new_block(__mem_pool_obj* pool_obj, unsigned long size) {
    Block* bp;
    SubBlock* sb;

    size = (size + BLOCK_OVERHEAD + 7) & ~7;

    if (size < NEW_BLOCK_MIN_SIZE) {
        size = NEW_BLOCK_MIN_SIZE;
    }

    bp = (Block*)__sys_alloc(size);

    if (bp == 0) {
        return 0;
    }

    bp->size_ = size | 3;
    ((SubBlock*)((char*)bp + size - 8))->size_ = bp->size_;
    sb = (SubBlock*)((char*)bp + sizeof(Block));
    SubBlock_construct(sb, size - BLOCK_OVERHEAD, bp, 0, 0);
    bp->max_size_ = size - BLOCK_OVERHEAD;
    Block_start(bp) = 0;
    Block_link(bp, sb);

    if (pool_obj->start_) {
        bp->prev_ = pool_obj->start_->prev_;
        bp->prev_->next_ = bp;
        bp->next_ = pool_obj->start_;
        pool_obj->start_->prev_ = bp;
        pool_obj->start_ = bp;
    } else {
        bp->next_ = bp->prev_ = pool_obj->start_ = bp;
    }

    return bp;
}

/*
 * allocate_from_var_pools
 *
 * Allocates from the variable-size blocks, adding a new Block when none has room.
 */
static void* allocate_from_var_pools(__mem_pool_obj* pool_obj, unsigned long size, unsigned long* max_size) {
    Block* bp;
    SubBlock* ptr;

    if (max_size) {
        *max_size = 0;
    }

    size = (size + SUBBLOCK_OVERHEAD + 7) & ~7;

    if (size < MIN_SUBBLOCK_SIZE) {
        size = MIN_SUBBLOCK_SIZE;
    }

    bp = pool_obj->start_ ? pool_obj->start_ : link_new_block(pool_obj, size);

    if (bp == 0) {
        return 0;
    }

    do {
        if (size <= bp->max_size_) {
            ptr = Block_subBlock(bp, size, max_size);

            if (ptr) {
                pool_obj->start_ = bp;
                goto done; /* both ways out share one return */
            }
        }

        bp = bp->next_;
    } while (bp != pool_obj->start_);

    bp = link_new_block(pool_obj, size);

    if (bp == 0) {
        return 0;
    }

    ptr = Block_subBlock(bp, size, max_size);

done:
    return SubBlock_client(ptr);
} // 0x80196124

/*
 * soft_allocate_from_var_pools
 *
 * Like allocate_from_var_pools but never adds a Block; max_size reports the
 * largest free area when nothing fits.
 */
static void* soft_allocate_from_var_pools(__mem_pool_obj* pool_obj, unsigned long size, unsigned long* max_size) {
    Block* bp;
    SubBlock* ptr;

    size = (size + SUBBLOCK_OVERHEAD + 7) & ~7;

    if (size < MIN_SUBBLOCK_SIZE) {
        size = MIN_SUBBLOCK_SIZE;
    }

    *max_size = 0;
    bp = pool_obj->start_;

    if (bp == 0) {
        return 0;
    }

    do {
        if (size <= bp->max_size_) {
            ptr = Block_subBlock(bp, size, 0);

            if (ptr) {
                pool_obj->start_ = bp;
                goto done; /* both ways out share one return */
            }
        }

        if (bp->max_size_ > SUBBLOCK_OVERHEAD && *max_size < bp->max_size_ - SUBBLOCK_OVERHEAD) {
            *max_size = bp->max_size_ - SUBBLOCK_OVERHEAD;
        }

        bp = bp->next_;
    } while (bp != pool_obj->start_);

    return 0;

done:
    return SubBlock_client(ptr);
} // 0x80196364

/*
 * FixBlock_construct
 *
 * Turns n bytes at start into a FixBlock: links it between prev and next and
 * threads its FixSubBlocks into a free list.
 */
static void FixBlock_construct(FixBlock* ths, FixBlock* prev, FixBlock* next, unsigned long index, void* start, unsigned long sz) {
    unsigned long csz = fix_pool_sizes[index];
    unsigned long n;
    unsigned long stride = csz + 4;
    unsigned long i;
    char* p = (char*)start;
    char* np;

    ths->prev_ = prev;
    ths->next_ = next;
    prev->next_ = ths;
    next->prev_ = ths;
    ths->client_size_ = csz;
    n = sz / stride;

    for (i = 0; i < n - 1; ++i) {
        np = p + stride;
        ((FixSubBlock*)p)->block_ = ths;
        ((FixSubBlock*)p)->next_ = (FixSubBlock*)np;
        p = np;
    }

    ((FixSubBlock*)p)->block_ = ths;
    ((FixSubBlock*)p)->next_ = 0;
    ths->start_ = (FixSubBlock*)start;
    ths->n_allocated_ = 0;
} // 0x80196440

/*
 * __msize_inline
 *
 * Usable size of an allocated client pointer.
 */
// byteptr: allocator header, the flag word in front of a client pointer (zeldaret/ss alloc.c classify)
#define classify(ptr) (*(unsigned long*)((char*)(ptr) - sizeof(unsigned long)) & 1)
#define FixSubBlock_size(ths) ((ths)->block_->client_size_)
#define __msize_inline(ptr) \
    (!classify(ptr) ? FixSubBlock_size(FixSubBlock_from_pointer(ptr)) : SubBlock_size(SubBlock_from_pointer(ptr)) - SUBBLOCK_OVERHEAD)

#define FixBlock_overhead sizeof(FixBlock)

/*
 * allocate_from_fixed_pools
 *
 * Allocates from the fixed-size pool that fits size, building a FixBlock from
 * the variable-size blocks when the pool has no free FixSubBlock left.
 */
static void* allocate_from_fixed_pools(__mem_pool_obj* pool_obj, unsigned long size, unsigned long* max_size) {
    unsigned long i = 0;
    FixStart* fs;
    FixSubBlock* p;

    while (size > fix_pool_sizes[i]) {
        ++i;
    }

    fs = &pool_obj->fix_start[i];
    if (fs->head_ == 0 || fs->head_->start_ == 0) {
        unsigned long cnt = FIX_BLOCK_DATA_SIZE / (fix_pool_sizes[i] + 4);
        unsigned long n;
        unsigned long avail;
        unsigned long size_received;
        unsigned long block_size;
        void* ptr;

        if (cnt > FIX_BLOCK_MAX_CLIENTS) {
            cnt = FIX_BLOCK_MAX_CLIENTS;
        }

        n = cnt;

        while (cnt >= FIX_BLOCK_MIN_CLIENTS) {
            ptr = soft_allocate_from_var_pools(pool_obj, cnt * (fix_pool_sizes[i] + 4) + FixBlock_overhead, &avail);

            if (ptr) {
                break;
            }

            if (avail > FixBlock_overhead) {
                cnt = (avail - FixBlock_overhead) / (fix_pool_sizes[i] + 4);
            } else {
                cnt = 0;
            }
        }

        if (ptr == 0) {
            for (;;) {
                ptr = allocate_from_var_pools(pool_obj, n * (fix_pool_sizes[i] + 4) + FixBlock_overhead, &size_received);

                if (ptr) {
                    break;
                }

                /* one client with its header, plus the FixBlock header */
                n = size_received / (fix_pool_sizes[i] + 0x18);

                if (n < FIX_BLOCK_MIN_CLIENTS) {
                    if (max_size) {
                        *max_size = 0;
                    }

                    return 0;
                }
            }
        }

        block_size = __msize_inline(ptr);

        if (fs->head_ == 0) {
            fs->tail_ = fs->head_ = (FixBlock*)ptr;
        }

        FixBlock_construct((FixBlock*)ptr, fs->tail_, fs->head_, i, (char*)ptr + FixBlock_overhead, block_size - FixBlock_overhead);
        fs->head_ = (FixBlock*)ptr;
    }

    p = fs->head_->start_;
    fs->head_->start_ = p->next_;
    ++fs->head_->n_allocated_;

    if (fs->head_->start_ == 0) {
        fs->head_ = fs->head_->next_;
        fs->tail_ = fs->tail_->next_;
    }

    if (max_size) {
        *max_size = fix_pool_sizes[i];
    }

    return FixSubBlock_client(p);
} // 0x80196564

/*
 * deallocate_from_var_pools
 *
 * Returns a variable-size allocation to its Block and gives the Block back to
 * the system heap when it becomes empty.
 */
static inline void deallocate_from_var_pools(__mem_pool_obj* pool_obj, void* ptr) {
    SubBlock* sb = SubBlock_from_pointer(ptr);
    Block* bp = SubBlock_block(sb);
    Block* next;

    Block_link(bp, sb);

    if (Block_empty(bp)) {
        next = bp->next_;

        if (next == bp) {
            next = 0;
        }

        if (pool_obj->start_ == bp) {
            pool_obj->start_ = next;
        }

        if (next) {
            next->prev_ = bp->prev_;
            next->prev_->next_ = next;
        }

        bp->next_ = 0;
        bp->prev_ = 0;
        __sys_free(bp);
    }
}

/*
 * deallocate_from_fixed_pools
 *
 * Returns a FixSubBlock to its FixBlock and releases the FixBlock when it
 * becomes empty.
 */
static void deallocate_from_fixed_pools(__mem_pool_obj* pool_obj, void* ptr, unsigned long size) {
    unsigned long i = 0;
    FixStart* fs;
    FixSubBlock* p;
    FixBlock* bp;

    while (size > fix_pool_sizes[i]) {
        ++i;
    }

    p = FixSubBlock_from_pointer(ptr);
    bp = p->block_;
    fs = &pool_obj->fix_start[i];

    if (bp->start_ == 0 && fs->head_ != bp) {
        if (fs->tail_ == bp) {
            fs->head_ = fs->head_->prev_;
            fs->tail_ = fs->tail_->prev_;
        } else {
            bp->prev_->next_ = bp->next_;
            bp->next_->prev_ = bp->prev_;
            bp->next_ = fs->head_;
            bp->prev_ = bp->next_->prev_;
            bp->prev_->next_ = bp;
            bp->next_->prev_ = bp;
            fs->head_ = bp;
        }
    }

    p->next_ = bp->start_;
    bp->start_ = p;

    if (--bp->n_allocated_ == 0) {
        if (fs->head_ == bp) {
            fs->head_ = bp->next_;
        }

        if (fs->tail_ == bp) {
            fs->tail_ = bp->prev_;
        }

        bp->prev_->next_ = bp->next_;
        bp->next_->prev_ = bp->prev_;

        if (fs->head_ == bp) {
            fs->head_ = 0;
        }

        if (fs->tail_ == bp) {
            fs->tail_ = 0;
        }

        deallocate_from_var_pools(pool_obj, bp);
    }
} // 0x8019676C

/*
 * __pool_resize_inplace
 *
 * Tries to make an allocation fit size bytes without moving it, merging with or
 * splitting off free memory behind it. Invented name: this function has no
 * recalled counterpart.
 */
static int __pool_resize_inplace(__mem_pool_obj* pool_obj, void* ptr, unsigned long size, unsigned long* max_size) {
    FixSubBlock* fp = FixSubBlock_from_pointer(ptr);
    unsigned long is_var = (unsigned long)fp->block_ & 1;
    unsigned long oldsize;
    unsigned long new_size;
    unsigned long cur_size;
    SubBlock* sb;
    SubBlock* np;

    if (is_var == 0) {
        oldsize = fp->block_->client_size_;
    } else {
        oldsize = SubBlock_size(SubBlock_from_pointer(ptr)) - SUBBLOCK_OVERHEAD;
    }

    if (size == oldsize) {
        if (max_size) {
            *max_size = oldsize;
        }

        return 1;
    }

    if (is_var) {
        if (size > MAX_RESIZE_REQUEST) {
            size = MAX_RESIZE_REQUEST;
        }

        new_size = (size + SUBBLOCK_OVERHEAD + 7) & ~7;

        if (new_size < MIN_SUBBLOCK_SIZE) {
            new_size = MIN_SUBBLOCK_SIZE;
        }

        sb = SubBlock_from_pointer(ptr);
        cur_size = oldsize + SUBBLOCK_OVERHEAD;
        np = SubBlock_neighbor(sb, cur_size);

        if (SubBlock_is_free(np) && new_size <= cur_size + SubBlock_size(np)) {
            SubBlock_merge_next(sb, &Block_start(SubBlock_block(sb)));
            cur_size = SubBlock_size(sb);
        }

        if (cur_size >= new_size + MIN_SUBBLOCK_SIZE) {
            SubBlock_split(sb, new_size);
            Block_link(SubBlock_block(sb), SubBlock_neighbor(sb, new_size));
            cur_size = SubBlock_size(sb);
        }

        if (max_size) {
            *max_size = cur_size - SUBBLOCK_OVERHEAD;
        }

        if (size > oldsize) {
            if (max_size && size > cur_size - SUBBLOCK_OVERHEAD && SubBlock_is_free(np)) {
                *max_size += SubBlock_size(np);
            }

            return cur_size - SUBBLOCK_OVERHEAD >= size;
        }

        return cur_size - SUBBLOCK_OVERHEAD < oldsize;
    }

    if (max_size) {
        *max_size = oldsize;
    }

    return 0;
} // 0x80196958

/*
 * __init_pool_obj
 *
 * Clears a pool.
 */
static inline void __init_pool_obj(__mem_pool* pool_obj) {
    memset(pool_obj, 0, sizeof(__mem_pool_obj));
}

/*
 * get_malloc_pool
 *
 * The pool behind malloc, set up on first use.
 */
static inline __mem_pool* get_malloc_pool(void) {
    if (!initted) {
        __init_pool_obj(&protopool);
        initted = 1;
    }

    return &protopool;
}

/*
 * __pool_alloc
 *
 * Allocation of size bytes, routed to the fixed or the variable-size pools.
 */
static inline void* __pool_alloc(__mem_pool* pool, unsigned long size) {
    __mem_pool_obj* pool_obj = (__mem_pool_obj*)pool;

    if (size > MAX_POOL_REQUEST) {
        return 0;
    }

    if (size <= MAX_FIX_CLIENT_SIZE) {
        return allocate_from_fixed_pools(pool_obj, size, 0);
    }

    return allocate_from_var_pools(pool_obj, size, 0);
}

/*
 * __pool_free
 *
 * Frees an allocation, routed to the fixed or the variable-size pools.
 */
static inline void __pool_free(__mem_pool* pool, void* ptr) {
    __mem_pool_obj* pool_obj;
    unsigned long size;

    if (ptr == 0) {
        return;
    }

    pool_obj = (__mem_pool_obj*)pool;
    size = __msize_inline(ptr);

    if (size <= MAX_FIX_CLIENT_SIZE) {
        deallocate_from_fixed_pools(pool_obj, ptr, size);
    } else {
        deallocate_from_var_pools(pool_obj, ptr);
    }
}

/*
 * __pool_realloc
 *
 * Resizes an allocation in place when it can, otherwise moves it.
 */
static inline void* __pool_realloc(__mem_pool* pool, void* ptr, unsigned long size) {
    __mem_pool_obj* pool_obj = (__mem_pool_obj*)pool;
    void* newptr;
    unsigned long oldsize;

    if (ptr == 0) {
        return __pool_alloc(pool, size);
    }

    if (size == 0) {
        __pool_free(pool, ptr);
        return 0;
    }

    oldsize = __msize_inline(ptr);

    if (__pool_resize_inplace(pool_obj, ptr, size, 0) || size <= oldsize) {
        return ptr;
    }

    newptr = __pool_alloc(pool, size);

    if (newptr) {
        memcpy(newptr, ptr, oldsize);
        __pool_free(pool, ptr);
    }

    return newptr;
}

/*
 * malloc
 */
void* malloc(size_t size) {
    if (size != 0) {
        return __pool_alloc(get_malloc_pool(), size);
    }

    return 0;
} // 0x80196BB8

/*
 * free
 */
void free(void* ptr) {
    __pool_free(get_malloc_pool(), ptr);
} // 0x80196C58

/*
 * realloc
 */
void* realloc(void* ptr, size_t size) {
    return __pool_realloc(get_malloc_pool(), ptr, size);
} // 0x80196D88

/*
 * calloc
 */
void* calloc(size_t nmemb, size_t size) {
    __mem_pool* pool = get_malloc_pool();
    void* ptr;

    size *= nmemb;
    ptr = __pool_alloc(pool, size);

    if (ptr) {
        memset(ptr, 0, size);
    }

    return ptr;
} // 0x801970A0
