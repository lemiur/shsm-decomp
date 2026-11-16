/*
 * fmod_buckethash.cpp: FMOD::BucketHash, the Event system's open-addressing hash table. Allocation tags
 * carry the original file's line numbers (docs/exceptions.md, "FMOD rulings (fmod-1)").
 */
// Unit name and boundary: Gormiti link map (fmod_event.a fmod_buckethash.obj)

#include <fmod/fmod_buckethash.h>
#include <fmod/fmod_memory.h>

namespace FMOD {

/*
====================
BucketHash::~BucketHash
====================
*/
BucketHash::~BucketHash() {
    release();
} // 0x8037BB68

/*
====================
BucketHash::init

Allocates the table, with room for numentries buckets at a load factor of 0.7.
====================
*/
FMOD_RESULT BucketHash::init( unsigned long numentries, KeyCompare keycompare, HashFunction hash ) {
    static const float LOAD_FACTOR = 0.7f;

    if ( !numentries ) {
        m_hashtable = NULL;
        return FMOD_OK;
    }

    m_size       = (unsigned long)( numentries / LOAD_FACTOR );
    m_keycompare = keycompare;
    m_hash       = hash;

#line 28
    m_hashtable = (void **)FMOD_Memory_Calloc( m_size * sizeof( void * ) );
    if ( !m_hashtable ) {
        return FMOD_ERR_MEMORY;
    }

    return FMOD_OK;
} // 0x8037BBE8

/*
====================
BucketHash::release

Frees the table.
====================
*/
FMOD_RESULT BucketHash::release() {
    if ( m_hashtable ) {
#line 41
        FMOD_Memory_Free( m_hashtable );
        m_hashtable = NULL;
    }

    return FMOD_OK;
} // 0x8037BCA4

/*
====================
BucketHash::moveTo

Makes dest the owner of this table and this one empty; what dest held is freed.
====================
*/
FMOD_RESULT BucketHash::moveTo( BucketHash *dest ) {
    void **oldhashtable = dest->m_hashtable;

    *dest = *this;
    *this = BucketHash();

    if ( oldhashtable ) {
#line 61
        FMOD_Memory_Free( oldhashtable );
    }

    return FMOD_OK;
} // 0x8037BD00

/*
====================
BucketHash::insert

Puts bucket in the first free slot from the one its key hashes to; fails when the table is full.
====================
*/
FMOD_RESULT BucketHash::insert( const void *key, void *bucket ) {
    if ( !m_hashtable ) {
        return FMOD_ERR_INTERNAL;
    }

    unsigned long hashindex = m_hash( key ) % m_size;
    unsigned long index     = hashindex;

    do {
        if ( !m_hashtable[index] ) {
            m_hashtable[index] = bucket;
            return FMOD_OK;
        }

        index++;
        index %= m_size;
    } while ( index != hashindex );

    return FMOD_ERR_INTERNAL;
} // 0x8037BD98

/*
====================
BucketHash::find

Returns the bucket the key compares equal to, or NULL.
====================
*/
void *BucketHash::find( const void *key ) const {
    if ( !m_hashtable ) {
        return NULL;
    }

    unsigned long hashindex = m_hash( key ) % m_size;
    unsigned long index     = hashindex;

    do {
        void *bucket = m_hashtable[index];
        if ( !bucket ) {
            return NULL;
        }

        if ( m_keycompare( key, bucket ) ) {
            return bucket;
        }

        index++;
        index %= m_size;
    } while ( index != hashindex );

    return NULL;
} // 0x8037BE48

/*
====================
BucketHash::hash_uint

The key is the address of an unsigned int, which is its own hash.
====================
*/
unsigned long BucketHash::hash_uint( const void *key ) {
    return *(const unsigned long *)key;
} // 0x8037BF0C

} // namespace FMOD
