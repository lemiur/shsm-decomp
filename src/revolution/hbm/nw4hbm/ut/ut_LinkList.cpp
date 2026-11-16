// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/ut/ut_LinkList.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/ut/ut_LinkList.cpp
#include "nw4hbm/ut/LinkList.h"

namespace nw4hbm {
namespace ut {
namespace detail {

/*
 * LinkListImpl::~LinkListImpl
 *
 * Unlinks every element; the elements themselves are not destroyed.
 */
LinkListImpl::~LinkListImpl() {
    Clear();
} // 0x803CDEE0

/*
 * LinkListImpl::Erase
 *
 * Unlinks the element at `it`; returns the iterator to the element after it.
 */
LinkListImpl::Iterator LinkListImpl::Erase(Iterator it) {
    Iterator itNext = it;
    ++itNext;
    return Erase(it, itNext);
} // 0x803CDF70

/*
 * LinkListImpl::Erase
 *
 * Unlinks the elements from `itFirst` up to, not including, `itLast`.
 */
LinkListImpl::Iterator LinkListImpl::Erase(Iterator itFirst, Iterator itLast) {
    LinkListNode* p = itFirst.mPointer;
    LinkListNode* pLast = itLast.mPointer;

    while (p != pLast) {
        LinkListNode* pNext = p->mNext;
        Erase(p);
        p = pNext;
    }

    return itLast;
}

/*
 * LinkListImpl::Clear
 *
 * Unlinks every element.
 */
void LinkListImpl::Clear() {
    Erase(GetBeginIter(), GetEndIter());
}

/*
 * LinkListImpl::Insert
 *
 * Links `p` in front of the element at `it`; returns the iterator to `p`.
 */
LinkListImpl::Iterator LinkListImpl::Insert(Iterator it, LinkListNode* p) {
    LinkListNode* pNext = it.mPointer;
    LinkListNode* pPrev = pNext->mPrev;

    p->mNext = pNext;
    p->mPrev = pPrev;
    pNext->mPrev = p;
    pPrev->mNext = p;
    mSize++;

    return p;
} // 0x803CDFC0

/*
 * LinkListImpl::Erase
 *
 * Unlinks `p` and clears its links; returns the iterator to the element after it.
 */
LinkListImpl::Iterator LinkListImpl::Erase(LinkListNode* p) {
    LinkListNode* pNext = p->mNext;
    LinkListNode* pPrev = p->mPrev;

    pNext->mPrev = pPrev;
    pPrev->mNext = pNext;
    mSize--;

    p->mNext = NULL;
    p->mPrev = NULL;

    return pNext;
}

} // namespace detail
} // namespace ut
} // namespace nw4hbm
