#ifndef NW4HBM_UT_LINK_LIST_H
#define NW4HBM_UT_LINK_LIST_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/LinkList.h

#include <revolution/types.h>

#include "nw4hbm/ut/inlines.h"

namespace nw4hbm {
namespace ut {

namespace detail {
class LinkListImpl;
}

/*
 * LinkListNode
 *
 * Node embedded in each element of a LinkList.
 */
class LinkListNode : private NonCopyable {
public:
    LinkListNode() : mNext(NULL), mPrev(NULL) {}

    LinkListNode* GetNext() const { return mNext; }
    LinkListNode* GetPrev() const { return mPrev; }

private:
    LinkListNode* mNext; // 0x00
    LinkListNode* mPrev; // 0x04

    friend class detail::LinkListImpl;
}; // size 0x08

namespace detail {

/*
 * LinkListImpl
 *
 * Untyped core of LinkList: a circular doubly linked list of LinkListNodes
 * whose sentinel is the embedded mNode.
 */
class LinkListImpl : private NonCopyable {
public:
    class Iterator {
    public:
        Iterator(LinkListNode* p) : mPointer(p) {}

        Iterator& operator++() {
            mPointer = mPointer->GetNext();
            return *this;
        }

        LinkListNode* operator->() const { return mPointer; }

        friend bool operator==(Iterator it1, Iterator it2) { return it1.mPointer == it2.mPointer; }
        friend bool operator!=(Iterator it1, Iterator it2) { return !(it1 == it2); }

    private:
        LinkListNode* mPointer; // 0x00

        friend class LinkListImpl;
    }; // size 0x04

    LinkListImpl() { Initialize_(); }
    ~LinkListImpl();

    Iterator GetBeginIter() { return mNode.GetNext(); }
    Iterator GetEndIter() { return &mNode; }

    u32 GetSize() const { return mSize; }
    bool IsEmpty() const { return mSize == 0; }

    Iterator Insert(Iterator it, LinkListNode* p);
    void PushFront(LinkListNode* p) { Insert(GetBeginIter(), p); }
    void PushBack(LinkListNode* p) { Insert(GetEndIter(), p); }

    Iterator Erase(Iterator it);
    Iterator Erase(Iterator itFirst, Iterator itLast);
    Iterator Erase(LinkListNode* p);
    void Clear();

    static Iterator GetIteratorFromPointer(LinkListNode* p) { return p; }

private:
    void Initialize_() {
        mSize = 0;
        mNode.mNext = &mNode;
        mNode.mPrev = &mNode;
    }

    u32 mSize;          // 0x00 number of elements
    LinkListNode mNode; // 0x04 sentinel: mNext is the first element, mPrev the last
}; // size 0x0C

} // namespace detail

/*
 * LinkList
 *
 * Typed view of LinkListImpl over elements of type T that embed a LinkListNode
 * at byte offset I.
 */
template <typename T, int I>
class LinkList : private detail::LinkListImpl {
public:
    class Iterator {
    public:
        Iterator(detail::LinkListImpl::Iterator it) : mIterator(it) {}

        Iterator& operator++() {
            ++mIterator;
            return *this;
        }

        Iterator operator++(int) {
            Iterator ret = *this;
            ++mIterator;
            return ret;
        }

        T& operator*() const { return *operator->(); }
        T* operator->() const { return GetPointerFromNode(mIterator.operator->()); }

        friend bool operator==(Iterator it1, Iterator it2) { return it1.mIterator == it2.mIterator; }
        friend bool operator!=(Iterator it1, Iterator it2) { return !(it1 == it2); }

    private:
        detail::LinkListImpl::Iterator mIterator; // 0x00

        friend class LinkList<T, I>;
    }; // size 0x04

    LinkList() {}

    Iterator GetBeginIter() { return LinkListImpl::GetBeginIter(); }
    Iterator GetEndIter() { return LinkListImpl::GetEndIter(); }

    Iterator Insert(Iterator it, T* p) { return LinkListImpl::Insert(it.mIterator, GetNodeFromPointer(p)); }
    void PushBack(T* p) { Insert(GetEndIter(), p); }

    Iterator Erase(Iterator it) { return LinkListImpl::Erase(it.mIterator); }
    Iterator Erase(T* p) { return LinkListImpl::Erase(GetNodeFromPointer(p)); }

    static Iterator GetIteratorFromPointer(T* p) { return LinkListImpl::GetIteratorFromPointer(GetNodeFromPointer(p)); }

    static LinkListNode* GetNodeFromPointer(T* p) {
        return reinterpret_cast<LinkListNode*>(reinterpret_cast<u8*>(p) + I);
    }

    static T* GetPointerFromNode(LinkListNode* p) {
        return reinterpret_cast<T*>(reinterpret_cast<u8*>(p) - I);
    }
}; // size 0x0C

} // namespace ut
} // namespace nw4hbm

// Visits each element of `list`, front to back, naming the iterator `it`.
// RULE-EXCEPTION: R31, `it` is the name of the iterator the macro declares, so it cannot be parenthesised.
#define NW4HBM_RANGE_FOR(it, list) \
    for (__typeof__((list).GetBeginIter()) it = (list).GetBeginIter(); it != (list).GetEndIter(); ++it)

// Visits each element of `list`, front to back, leaving the advance to the body so that it can
// take the next iterator before it erases the current element (`it++`).
// RULE-EXCEPTION: R31, `it` is the name of the iterator the macro declares, so it cannot be parenthesised.
#define NW4HBM_RANGE_FOR_NO_AUTO_INC(it, list)     for (__typeof__((list).GetBeginIter()) it = (list).GetBeginIter(); it != (list).GetEndIter();)

#endif // NW4HBM_UT_LINK_LIST_H
