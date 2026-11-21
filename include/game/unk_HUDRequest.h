#ifndef GAME_UNK_HUDREQUEST_H
#define GAME_UNK_HUDREQUEST_H

// unit name unconfirmed: header and type names invented.

#include <revolution/types.h>
#include <stddef.h>

/*
 * A reference-counted request handed to the HUD controller (unk_HUDController::fn_800E50E8),
 * which shows its element until the request reports itself done. The base class counts its
 * lifetime down by the frame time and is done once that has run out. Its vtable (0x803E6194)
 * and out-of-line copies (destructor 0x80073514, fn_80072D38, fn_80072D1C) are emitted in the
 * player's unit (not yet reconstructed), which constructs one at 0x8006E660. vtable size 0x14.
 * Names invented.
 */
class unk_HUDRequest {
public:
                    unk_HUDRequest( float lifetime ) :
                        refCount( 0 ),
                        timeLeft( lifetime ) {}
    virtual         ~unk_HUDRequest() {}
    /* Counts the lifetime down by the frame time dt. */
    virtual void    fn_80072D38( float dt ) {
                        timeLeft -= dt;
                    }
    /* True once the request has ended and its element can go. */
    virtual bool    fn_80072D1C() const {
                        return timeLeft <= 0.0f;
                    }

    void            AddRef() {
                        refCount++;
                    }
    void            Release() {
                        if ( --refCount == 0 ) {
                            delete this;
                        }
                    }

    int             refCount;   // 0x4, references held by unk_RefPtr
    float           timeLeft;   // 0x8, seconds
};

/*
 * An owning reference to a reference-counted object (one with AddRef and Release). Name
 * invented.
 */
template< class T >
class unk_RefPtr {
public:
                    unk_RefPtr( T* p = NULL ) :
                        ptr( p ) {
                        if ( ptr != NULL ) {
                            ptr->AddRef();
                        }
                    }
                    unk_RefPtr( const unk_RefPtr& other ) :
                        ptr( other.ptr ) {
                        if ( ptr != NULL ) {
                            ptr->AddRef();
                        }
                    }
                    ~unk_RefPtr() {
                        if ( ptr != NULL ) {
                            ptr->Release();
                        }
                    }

    unk_RefPtr&     operator=( const unk_RefPtr& other ) {
                        if ( other.ptr != NULL ) {
                            other.ptr->AddRef();
                        }
                        T* old = ptr;
                        ptr = other.ptr;
                        if ( old != NULL ) {
                            old->Release();
                        }
                        return *this;
                    }

    T*              operator->() const {
                        return ptr;
                    }
                    operator T*() const {
                        return ptr;
                    }

private:
    T*              ptr;        // 0x0
};

#endif
