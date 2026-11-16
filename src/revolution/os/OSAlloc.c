#include <revolution/os.h>

#define ALIGNMENT 32

/* Every block, free or allocated, starts with a Cell header. */
#define HEADERSIZE OSRoundUp32B(sizeof(Cell))

/* The smallest remainder worth splitting off as a free block. */
#define MINOBJSIZE (HEADERSIZE + ALIGNMENT)

#define ROUND(n, a) (((u32)(n) + (a) - 1) & ~((a) - 1))

struct HeapDesc {
    long size;              /* bytes; negative while the slot is unused */
    Cell* free;
    Cell* allocated;
};

volatile OSHeapHandle __OSCurrHeap = -1; // volatile: declared so by the SDK header

static struct HeapDesc* HeapArray;
static int NumHeaps;
static void* ArenaStart;
static void* ArenaEnd;

static Cell* DLAddFront(Cell* list, Cell* cell) {
    cell->next = list;
    cell->prev = NULL;
    if (list) {
        list->prev = cell;
    }
    return cell;
}

static Cell* DLExtract(Cell* list, Cell* cell) {
    if (cell->next) {
        cell->next->prev = cell->prev;
    }
    if (cell->prev == NULL) {
        return cell->next;
    } else {
        cell->prev->next = cell->next;
        return list;
    }
}

/*
 * DLInsert
 *
 * Inserts cell into an address-ordered free list, merging it with the
 * neighbouring blocks it touches. Returns the new list head.
 */
static Cell* DLInsert(Cell* list, Cell* cell) {
    Cell* prev;
    Cell* next;

    for (next = list, prev = NULL; next; prev = next, next = next->next) {
        if (cell <= next) {
            break;
        }
    }

    cell->next = next;
    cell->prev = prev;
    if (next) {
        next->prev = cell;
        if ((char*)cell + cell->size == (char*)next) {
            cell->size += next->size;
            cell->next = next = next->next;
            if (next) {
                next->prev = cell;
            }
        }
    }
    if (prev) {
        prev->next = cell;
        if ((char*)prev + prev->size == (char*)cell) {
            prev->size += cell->size;
            prev->next = next;
            if (next) {
                next->prev = prev;
            }
        }
        return list;
    } else {
        return cell;
    }
} // 0x802126A0

/*
 * OSAllocFromHeap
 *
 * First fit: takes the first free block large enough and splits off the
 * remainder when it can hold another block.
 */
void* OSAllocFromHeap(OSHeapHandle heap, u32 size) {
    struct HeapDesc* hd;
    Cell* cell;
    Cell* newCell;
    long leftoverSize;

    hd = &HeapArray[heap];
    size += HEADERSIZE;
    size = ROUND(size, ALIGNMENT);

    for (cell = hd->free; cell != NULL; cell = cell->next) {
        if ((long)size <= cell->size) {
            break;
        }
    }

    if (cell == NULL) {
        return NULL;
    }

    leftoverSize = cell->size - (long)size;
    if (leftoverSize < MINOBJSIZE) {
        hd->free = DLExtract(hd->free, cell);
    } else {
        cell->size = (long)size;

        newCell = (Cell*)((char*)cell + size);
        newCell->size = leftoverSize;
        newCell->prev = cell->prev;
        newCell->next = cell->next;
        if (newCell->next != NULL) {
            newCell->next->prev = newCell;
        }
        if (newCell->prev != NULL) {
            newCell->prev->next = newCell;
        } else {
            hd->free = newCell;
        }
    }

    hd->allocated = DLAddFront(hd->allocated, cell);

    return (void*)((char*)cell + HEADERSIZE);
} // 0x80212750

/*
 * OSFreeToHeap
 */
void OSFreeToHeap(OSHeapHandle heap, void* ptr) {
    struct HeapDesc* hd;
    Cell* cell;

    cell = (Cell*)((char*)ptr - HEADERSIZE);
    hd = &HeapArray[heap];
    hd->allocated = DLExtract(hd->allocated, cell);
    hd->free = DLInsert(hd->free, cell);
} // 0x80212850

/*
 * OSSetCurrentHeap
 *
 * Returns the previous current heap.
 */
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap) {
    OSHeapHandle prev;

    prev = __OSCurrHeap;
    __OSCurrHeap = heap;
    return prev;
} // 0x802128D0

/*
 * OSInitAlloc
 *
 * Places the heap descriptor array at the start of the arena. Returns the
 * first free address after it.
 */
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps) {
    u32 arraySize;
    OSHeapHandle i;

    arraySize = sizeof(struct HeapDesc) * maxHeaps;
    HeapArray = arenaStart;
    NumHeaps = maxHeaps;

    for (i = 0; i < NumHeaps; i++) {
        struct HeapDesc* hd = &HeapArray[i];

        hd->size = -1;
        hd->free = hd->allocated = NULL;
    }

    __OSCurrHeap = -1;

    arenaStart = (void*)((u32)((char*)HeapArray + arraySize));
    arenaStart = (void*)OSRoundUp32B(arenaStart);

    ArenaStart = arenaStart;
    ArenaEnd = (void*)OSRoundDown32B(arenaEnd);

    return arenaStart;
} // 0x802128E0

/*
 * OSCreateHeap
 *
 * Takes the first unused descriptor and makes [start, end) one free
 * block. Returns -1 when every descriptor is in use.
 */
OSHeapHandle OSCreateHeap(void* start, void* end) {
    OSHeapHandle heap;
    struct HeapDesc* hd;
    Cell* cell;

    start = (void*)OSRoundUp32B(start);
    end = (void*)OSRoundDown32B(end);

    for (heap = 0; heap < NumHeaps; heap++) {
        hd = &HeapArray[heap];
        if (hd->size < 0) {
            hd->size = (char*)end - (char*)start;

            cell = (Cell*)start;
            cell->prev = NULL;
            cell->next = NULL;
            cell->size = hd->size;

            hd->free = cell;
            hd->allocated = NULL;

            return heap;
        }
    }

    return -1;
} // 0x80212950
