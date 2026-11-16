// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_picture.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_picture.cpp
#include "nw4hbm/lyt/picture.h"

#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/material.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/inlines.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

#include <revolution/tpl.h>

#include <new>

namespace nw4hbm {
namespace lyt {

const ut::detail::RuntimeTypeInfo Picture::typeInfo(&Pane::typeInfo);

/*
 * Picture::Picture
 *
 * Builds the picture from its data block: corner colours, texture coordinates and material.
 */
Picture::Picture(const res::Picture* pResPic, const ResBlockSet& resBlockSet) : Pane(pResPic) {
    u8 texCoordNum = ut::Min<u8>(pResPic->texCoordNum, 8);

    Init(texCoordNum);

    for (int i = 0; i < VERTEXCOLOR_MAX; ++i) {
        mVtxColors[i] = pResPic->vtxCols[i];
    }

    if (texCoordNum != 0 && !mTexCoordAry.IsEmpty()) {
        mTexCoordAry.Copy(pResPic + 1, texCoordNum);
    }

    if (Material* pMemMaterial = static_cast<Material*>(Layout::AllocMemory(sizeof *pMemMaterial))) {
        const u32* matOffsTbl =
            detail::ConvertOffsToPtr<u32>(resBlockSet.pMaterialList, sizeof *resBlockSet.pMaterialList);
        const res::Material* pResMaterial = detail::ConvertOffsToPtr<res::Material>(
            resBlockSet.pMaterialList, matOffsTbl[pResPic->materialIdx]);

        mpMaterial = new (pMemMaterial) Material(pResMaterial, resBlockSet);
    }
} // 0x803C80D0

/*
 * Picture::Init
 */
void Picture::Init(u8 texNum) {
    if (texNum != 0) {
        ReserveTexCoord(texNum);
    }
} // no copy in retail (inlined)

/*
 * Picture::~Picture
 */
Picture::~Picture() {
    if (mpMaterial && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
        mpMaterial = NULL;
    }

    mTexCoordAry.Free();
} // 0x803C8200

/*
 * Picture::Append
 *
 * Adds the first texture of a TPL palette.
 */
void Picture::Append(TPLPalette* pTplRes) {
    GXTexObj texObj;
    detail::InitGXTexObjFromTPL(&texObj, pTplRes, 0);

    Append(texObj);
} // 0x803C82D0

/*
 * Picture::Append
 *
 * Adds a texture to the material and one set of texture coordinates for it. A picture with no
 * size yet takes the size of its first texture.
 */
void Picture::Append(const GXTexObj& texObj) {
    if (mpMaterial->GetTextureNum() >= mpMaterial->GetTextureCap() ||
        mpMaterial->GetTextureNum() >= mpMaterial->GetTexCoordGenCap()) {
        return;
    }

    u8 texIdx = mpMaterial->GetTextureNum();

    mpMaterial->SetTextureNum(texIdx + 1);
    mpMaterial->SetTexture(texIdx, texObj);
    mpMaterial->SetTexCoordGenNum(mpMaterial->GetTextureNum());
    mpMaterial->SetTexCoordGen(texIdx, TexCoordGen());

    SetTexCoordNum(mpMaterial->GetTextureNum());

    if (mSize == Size(0.0f, 0.0f) && mpMaterial->GetTextureNum() == 1) {
        mSize = detail::GetTextureSize(mpMaterial, 0);
    }
} // 0x803C8320

/*
 * Picture::ReserveTexCoord
 */
void Picture::ReserveTexCoord(u8 num) {
    mTexCoordAry.Reserve(num);
} // no copy in retail (inlined)

/*
 * Picture::SetTexCoordNum
 */
void Picture::SetTexCoordNum(u8 num) {
    mTexCoordAry.SetSize(num);
} // no copy in retail (inlined)

/*
 * Picture::GetVtxColor
 */
ut::Color Picture::GetVtxColor(u32 idx) const {
    return mVtxColors[idx];
} // 0x803C8490

/*
 * Picture::SetVtxColor
 */
void Picture::SetVtxColor(u32 idx, ut::Color value) {
    mVtxColors[idx] = value;
} // 0x803C84B0

/*
 * Picture::GetVtxColorElement
 */
u8 Picture::GetVtxColorElement(u32 idx) const {
    return detail::GetVtxColorElement(mVtxColors, idx);
} // 0x803C84E0

/*
 * Picture::SetVtxColorElement
 */
void Picture::SetVtxColorElement(u32 idx, u8 value) {
    detail::SetVtxColorElement(mVtxColors, idx, value);
} // 0x803C8500

/*
 * Picture::DrawSelf
 *
 * Draws the pane's quad with its material, modulating by the corner colours when they are not
 * all opaque white.
 */
void Picture::DrawSelf(const DrawInfo& drawInfo) {
    if (!mpMaterial) {
        return;
    }

    LoadMtx(drawInfo);

    bool bUseVtxCol = mpMaterial->SetupGX(detail::IsModulateVertexColor(mVtxColors, mGlbAlpha), mGlbAlpha);

    detail::SetVertexFormat(bUseVtxCol, mTexCoordAry.GetSize());

    detail::DrawQuad(GetVtxPos(), mSize, mTexCoordAry.GetSize(), mTexCoordAry.GetArray(),
                     bUseVtxCol ? mVtxColors : NULL, mGlbAlpha);
} // 0x803C8520

} // namespace lyt
} // namespace nw4hbm
