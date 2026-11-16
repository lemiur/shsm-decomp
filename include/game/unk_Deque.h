#ifndef GAME_UNK_DEQUE_H
#define GAME_UNK_DEQUE_H

// unit name unconfirmed: header name invented. The template is inline in every user
// (unk_8000A040's pad queues, CSequencePuzzleLogic's input history).

#include <climax/Memory.h>

/* Sets every element of [first, last) to value. Name invented. */
template< class T >
inline void unk_Fill( T* first, T* last, const T& value ) {
    for ( ; first != last; ++first ) {
        *first = value;
    }
}

/* Copies [first, last) to dest. Name invented. */
template< class T >
inline void unk_Copy( T* first, T* last, T* dest ) {
    for ( ; first != last; ++first, ++dest ) {
        *dest = *first;
    }
}

/* Copies [first, last) backwards to the range ending at destLast. Name invented. */
template< class T >
inline void unk_CopyBackward( T* first, T* last, T* destLast ) {
    while ( last != first ) {
        *--destLast = *--last;
    }
}

/* The first element of [first, last) equal to value, or last. Name invented. */
template< class Iterator, class T >
inline Iterator unk_Find( Iterator first, Iterator last, const T& value ) {
    while ( first != last && *first != value ) {
        ++first;
    }
    return first;
}

/*
 * A double-ended queue of T in blocks of BLOCK_SIZE elements, reached through a map of
 * block pointers. Name and member names invented.
 */
template< class T, int BLOCK_SIZE >
class unk_Deque {
public:
    class iterator {
    public:
                    iterator( T** node, T* cur ) : node( node ), cur( cur ) {}

        iterator&   operator++() {
            T* last = *node + BLOCK_SIZE;
            if ( ++cur >= last ) {
                unsigned int offset = cur - last;
                node += offset / BLOCK_SIZE + 1;
                cur = *node + offset % BLOCK_SIZE;
            }
            return *this;
        }
        bool        operator==( const iterator& other ) const { return cur == other.cur; }
        bool        operator!=( const iterator& other ) const { return cur != other.cur; }
        int         operator-( const iterator& other ) const {
            int n = ( node - other.node + 1 ) * BLOCK_SIZE;
            if ( n != 0 ) {
                n -= other.cur - *other.node;
                n -= BLOCK_SIZE - ( cur - *node );
            }
            return n;
        }
        T&          operator*() const { return *cur; }
        T*          operator->() const { return cur; }

        T**         node;
        T*          cur;
    };

    unk_Deque() {
        Init();
    }

    // Destroys the elements, then frees the storage (the SGI STL deque's destructor and base
    // destructor in one).
    ~unk_Deque() {
        DestroyAll();
        FreeMap();
    }

    /* Removes every element and returns to one empty block. Name invented. */
    void        clear() {
        Free();
        Init();
    }

    /* Runs the destructor of every element. Name invented. */
    void        DestroyAll() {
        for ( iterator it = begin(); it != end(); ++it ) {
            it->~T();
        }
    }

    iterator    begin() const { return iterator( startNode, startCur ); }
    iterator    end() const { return iterator( finishNode, finishCur ); }
    int         size() const { return end() - begin(); }

    T&          operator[]( int n ) const {
        unsigned int offset = ( startCur - *startNode ) + n;
        return startNode[offset / BLOCK_SIZE][offset % BLOCK_SIZE];
    }

    /* Inserts value before the first element; takes the spare block after the last one, or a new
       block, when the first block is full, and grows or recentres the map when it has no room. */
    void        push_front( const T& value ) {
        if ( startCur == *startNode ) {
            T* block;
            T** spare = finishNode + 1;
            if ( spare < mapEnd && ( block = *spare ) != NULL ) {
                *spare = NULL;
            } else {
                block = new( GAME_HEAP, GAME_HEAP_FLAGS ) T[BLOCK_SIZE];
            }
            if ( startNode == mapBegin ) {
                if ( finishNode + 1 == mapEnd ) {
                    int oldSize = mapEnd - mapBegin;
                    int newSize = oldSize * 2 + 1;
                    T** newMap = newSize * sizeof( T* ) != 0 ? new( GAME_HEAP, GAME_HEAP_FLAGS ) T*[newSize] : NULL;
                    T** newStart = newMap + ( newSize - oldSize );
                    unk_Fill( newMap, newStart, ( T* )NULL );
                    unk_Copy( startNode, finishNode + 1, newStart );
                    SafeDeleteArray( mapBegin );
                    finishNode = newStart + ( finishNode - startNode );
                    startNode = newStart;
                    mapBegin = newMap;
                    mapEnd = newMap + newSize;
                    unk_Fill( finishNode + 1, mapEnd, ( T* )NULL );
                } else {
                    int shift = mapEnd - finishNode - 1;
                    T** newStart = startNode + shift;
                    unk_CopyBackward( startNode, finishNode + 1, newStart + ( finishNode + 1 - startNode ) );
                    startNode += shift;
                    finishNode += shift;
                    unk_Fill( mapBegin, startNode, ( T* )NULL );
                }
            }
            --startNode;
            *startNode = block;
            startCur = *startNode + BLOCK_SIZE;
        }
        new( --startCur ) T( value );
    }

    /* Appends value after the last element; moves on to the next block when the last one fills,
       growing the map, or moving the blocks to its start, when it has no room. */
    void        push_back( const T& value ) {
        new( finishCur++ ) T( value );
        if ( finishCur >= *finishNode + BLOCK_SIZE ) {
            if ( ++finishNode == mapEnd ) {
                if ( startNode == mapBegin ) {
                    int oldSize = mapEnd - mapBegin;
                    int newSize = oldSize * 2 + 1;
                    T** newMap = newSize * sizeof( T* ) != 0 ? new( GAME_HEAP, GAME_HEAP_FLAGS ) T*[newSize] : NULL;
                    unk_Copy( mapBegin, mapEnd, newMap );
                    SafeDeleteArray( mapBegin );
                    startNode = newMap + ( startNode - mapBegin );
                    finishNode = newMap + ( finishNode - mapBegin );
                    mapBegin = newMap;
                    mapEnd = newMap + newSize;
                    unk_Fill( finishNode, mapEnd, ( T* )NULL );
                } else {
                    int shift = startNode - mapBegin;
                    unk_Copy( startNode, finishNode, mapBegin );
                    startNode -= shift;
                    finishNode -= shift;
                    unk_Fill( finishNode, mapEnd, ( T* )NULL );
                }
            }
            if ( *finishNode == NULL ) {
                *finishNode = new( GAME_HEAP, GAME_HEAP_FLAGS ) T[BLOCK_SIZE];
            }
            finishCur = *finishNode;
        }
    }

    /* Removes the last element; frees the spare block after the last one when its block empties. */
    void        pop_back() {
        if ( finishCur == *finishNode && finishNode != startNode ) {
            if ( finishNode + 1 < mapEnd && finishNode[1] != NULL ) {
                SafeDeleteArray( finishNode[1] );
                finishNode[1] = NULL;
            }
            --finishNode;
            finishCur = *finishNode + BLOCK_SIZE;
        }
        --finishCur;
    }

    T**         mapBegin;
    T**         mapEnd;
    T**         startNode;
    T**         finishNode;
    T*          startCur;
    T*          finishCur;

private:
    /* Allocates a map of three block pointers and one empty block in the middle. Name invented. */
    void        Init() {
        mapBegin = new( GAME_HEAP, GAME_HEAP_FLAGS ) T*[3];
        mapEnd = mapBegin + 3;
        unk_Fill( mapBegin, mapEnd, ( T* )NULL );
        startNode = mapBegin + 1;
        *startNode = new( GAME_HEAP, GAME_HEAP_FLAGS ) T[BLOCK_SIZE];
        finishNode = startNode;
        finishCur = startCur = *startNode;
    }

    /* Destroys the elements and frees the blocks and the map. Name invented. */
    void        Free() {
        DestroyAll();
        FreeMap();
    }

    /* Frees the blocks and the map. Name invented. */
    void        FreeMap() {
        for ( T** n = mapBegin; n != mapEnd; ++n ) {
            if ( *n != NULL ) {
                SafeDeleteArray( *n );
            }
        }
        SafeDeleteArray( mapBegin );
    }
};

#endif
