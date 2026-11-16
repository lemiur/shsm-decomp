#ifndef NW4HBM_LYT_BOUNDING_H
#define NW4HBM_LYT_BOUNDING_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/bounding.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_bounding.h

#include <revolution/types.h>
#include "nw4hbm/lyt/pane.h"

namespace nw4hbm {
namespace lyt {

struct ResBlockSet;

namespace res {

static const u32 SIGNATURE_BOUNDING_BLOCK = 'bnd1';

/*
 * res::Bounding
 *
 * Layout-file data block of a bounding pane ("bnd1"): a plain pane block.
 */
struct Bounding : public Pane {}; // size 0x4C

} // namespace res

/*
 * Bounding
 *
 * A pane with no visual of its own, only a size; it draws an outline in debug draw mode.
 */
class Bounding : public Pane {
public:
    Bounding(const res::Bounding* pBlock, const ResBlockSet& resBlockSet);
    virtual ~Bounding();

    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const { return &typeInfo; }

    virtual void DrawSelf(const DrawInfo& drawInfo);

    static const ut::detail::RuntimeTypeInfo typeInfo;
}; // size 0xD4

} // namespace lyt
} // namespace nw4hbm

#endif
