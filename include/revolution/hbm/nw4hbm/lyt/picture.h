#ifndef NW4HBM_LYT_PICTURE_H
#define NW4HBM_LYT_PICTURE_H

// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/picture.h
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/include/nw4hbm/lyt/lyt_picture.h

#include <revolution/types.h>
#include <revolution/tpl.h>
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/ut/Color.h"

namespace nw4hbm {
namespace lyt {

struct ResBlockSet;

namespace res {

static const u32 SIGNATURE_PICTURE_BLOCK = 'pic1';

/*
 * res::Picture
 *
 * Layout-file data block of a picture pane ("pic1"): the pane block, the colours of its four
 * corners and the index of its material; its texture coordinates follow.
 */
struct Picture : public Pane {
    u32 vtxCols[4];  // 0x4C
    u16 materialIdx; // 0x5C
    u8 texCoordNum;  // 0x5E
    u8 padding[1];   // 0x5F // pad: SDK res::Picture.padding
}; // size 0x60

} // namespace res

/*
 * Picture
 *
 * A pane that draws a quad textured by its material, with a colour at each corner.
 */
class Picture : public Pane {
public:
    // Number of corners, and so of vertex colours.
    enum {
        VERTEXCOLOR_MAX = 4,
    };

    Picture(u8 texNum);
    Picture(TPLPalette* pTplRes);
    Picture(const GXTexObj& texObj);
    Picture(const res::Picture* pResPic, const ResBlockSet& resBlockSet);
    virtual ~Picture();

    virtual const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const { return &typeInfo; }

    virtual void DrawSelf(const DrawInfo& drawInfo);
    virtual ut::Color GetVtxColor(u32 idx) const;
    virtual void SetVtxColor(u32 idx, ut::Color value);
    virtual u8 GetVtxColorElement(u32 idx) const;
    virtual void SetVtxColorElement(u32 idx, u8 value);

    virtual void Append(TPLPalette* pTplRes);
    virtual void Append(const GXTexObj& texObj);

    u8 GetTexCoordNum() const;
    void GetTexCoord(u32 idx, math::VEC2* coords) const;

    void SetTexCoordNum(u8 num);
    void SetTexCoord(u32 idx, const math::VEC2* coords);

    void Init(u8 texNum);
    void ReserveTexCoord(u8 num);

    static const ut::detail::RuntimeTypeInfo typeInfo;

private:
    ut::Color mVtxColors[VERTEXCOLOR_MAX]; // 0xD4
    detail::TexCoordAry mTexCoordAry;      // 0xE4
}; // size 0xEC

} // namespace lyt
} // namespace nw4hbm

#endif
