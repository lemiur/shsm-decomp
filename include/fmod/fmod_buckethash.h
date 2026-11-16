/*
 * FMOD::BucketHash: the Event system's open-addressing hash table of pointers. A bucket is whatever the
 * table's key-compare and hash callbacks understand; the table stores the pointers and owns only its
 * array. Layout: Gormiti DWARF (sibling.exe types BucketHash), size 0x10.
 */
#ifndef _FMOD_BUCKETHASH_H
#define _FMOD_BUCKETHASH_H

#include <fmod/fmod.h>

namespace FMOD {

class BucketHash {
public:
    typedef bool          ( *KeyCompare )( const void *key, void *bucket );
    typedef unsigned long ( *HashFunction )( const void *key );

    void         **m_hashtable;     // 0x00, m_size pointers, NULL for an empty slot
    unsigned long  m_size;          // 0x04
    KeyCompare     m_keycompare;    // 0x08
    HashFunction   m_hash;          // 0x0C

    BucketHash() {
        m_hashtable  = 0;
        m_size       = 0;
        m_keycompare = 0;
        m_hash       = 0;
    }
    ~BucketHash();

    // Sizes the table for numentries buckets and installs the callbacks.
    FMOD_RESULT init( unsigned long numentries, KeyCompare keycompare, HashFunction hash );
    FMOD_RESULT release();
    // Hands the table over to dest (freeing dest's old one), leaving this empty.
    FMOD_RESULT moveTo( BucketHash *dest );
    FMOD_RESULT insert( const void *key, void *bucket );
    void       *find( const void *key ) const;

    // A hash function for keys that point to an unsigned int.
    static unsigned long hash_uint( const void *key );
};

} // namespace FMOD

#endif /* _FMOD_BUCKETHASH_H */
