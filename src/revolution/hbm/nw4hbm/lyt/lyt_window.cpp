// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/hbm/nw4hbm/lyt/lyt_window.cpp (this drop)
// Reference: ThePlayerRolo/sdk_2009-12-11 source/homebuttonLib/nw4hbm/lyt/lyt_window.cpp
// Reference: xbret/xenoblade libs/RVL_SDK/src/revolution/hbm/nw4hbm/lyt/lyt_window.cpp
#include "nw4hbm/lyt/window.h"

#include "nw4hbm/lyt/animation.h"
#include "nw4hbm/lyt/common.h"
#include "nw4hbm/lyt/layout.h"
#include "nw4hbm/lyt/material.h"
#include "nw4hbm/lyt/pane.h"
#include "nw4hbm/lyt/types.h"
#include "nw4hbm/math/types.h"
#include "nw4hbm/ut/Color.h"
#include "nw4hbm/ut/inlines.h"
#include "nw4hbm/ut/LinkList.h"
#include "nw4hbm/ut/RuntimeTypeInfo.h"

#include <new>

namespace {

using namespace nw4hbm;
using namespace nw4hbm::lyt;

/*
 * TextureFlipInfo
 *
 * Texture coordinates of the four corners (LT, RT, LB, RB) of a frame part under one texture
 * flip, and which coordinate (x or y) each axis of the part reads.
 */
struct TextureFlipInfo {
    u8 coords[4][2]; // 0x00
    u8 idx[2];       // 0x08
}; // size 0x0A

/*
 * GetTexutreFlipInfo
 *
 * The corner coordinates for a texture flip (the misspelling is the SDK's).
 */
const TextureFlipInfo& GetTexutreFlipInfo(u8 textureFlip) {
    static TextureFlipInfo flipInfos[] = {
        {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}, {0, 1}}, // no flip
        {{{1, 0}, {0, 0}, {1, 1}, {0, 1}}, {0, 1}}, // horizontal flip
        {{{0, 1}, {1, 1}, {0, 0}, {1, 0}}, {0, 1}}, // vertical flip
        {{{0, 1}, {0, 0}, {1, 1}, {1, 0}}, {1, 0}}, // 90 degrees clockwise
        {{{1, 1}, {0, 1}, {1, 0}, {0, 0}}, {0, 1}}, // 180 degrees
        {{{1, 0}, {1, 1}, {0, 0}, {0, 1}}, {1, 0}}, // 270 degrees clockwise
    };

    return flipInfos[textureFlip];
} // no copy in retail (inlined)

/*
 * GetLTFrameSize
 *
 * Position and size of the top-left frame part (it runs along the top edge).
 */
void GetLTFrameSize(math::VEC2* pPt, Size* pSize, const math::VEC2& basePt, const Size& winSize,
                    const WindowFrameSize& frameSize) {
    *pPt = basePt;

    pSize->width = winSize.width - frameSize.r;
    pSize->height = frameSize.t;
} // no copy in retail (inlined)

/*
 * GetLTTexCoord
 */
void GetLTTexCoord(math::VEC2* texCds, const Size& polSize, const Size& texSize, u8 textureFlip) {
    const TextureFlipInfo& flipInfo = GetTexutreFlipInfo(textureFlip);
    int ix = flipInfo.idx[0];
    int iy = flipInfo.idx[1];
    const math::VEC2 tSz(texSize.width, texSize.height);

    texCds[0][ix] = texCds[2][ix] = flipInfo.coords[0][ix];
    texCds[0][iy] = texCds[1][iy] = flipInfo.coords[0][iy];

    texCds[3][ix] = texCds[1][ix] =
        flipInfo.coords[0][ix] + polSize.width / (tSz[ix] * (flipInfo.coords[1][ix] - flipInfo.coords[0][ix]));

    texCds[3][iy] = texCds[2][iy] =
        flipInfo.coords[0][iy] + polSize.height / (tSz[iy] * (flipInfo.coords[2][iy] - flipInfo.coords[0][iy]));
} // no copy in retail (inlined)

/*
 * GetRTFrameSize
 *
 * Position and size of the top-right frame part (it runs down the right edge).
 */
void GetRTFrameSize(math::VEC2* pPt, Size* pSize, const math::VEC2& basePt, const Size& winSize,
                    const WindowFrameSize& frameSize) {
    *pPt = math::VEC2(basePt.x + winSize.width - frameSize.r, basePt.y);

    pSize->width = frameSize.r;
    pSize->height = winSize.height - frameSize.b;
} // no copy in retail (inlined)

/*
 * GetRTTexCoord
 */
void GetRTTexCoord(math::VEC2* texCds, const Size& polSize, const Size& texSize, u8 textureFlip) {
    const TextureFlipInfo& flipInfo = GetTexutreFlipInfo(textureFlip);
    int ix = flipInfo.idx[0];
    int iy = flipInfo.idx[1];
    const math::VEC2 tSz(texSize.width, texSize.height);

    texCds[1][ix] = texCds[3][ix] = flipInfo.coords[1][ix];
    texCds[1][iy] = texCds[0][iy] = flipInfo.coords[1][iy];

    texCds[2][ix] = texCds[0][ix] =
        flipInfo.coords[1][ix] + polSize.width / (tSz[ix] * (flipInfo.coords[0][ix] - flipInfo.coords[1][ix]));

    texCds[2][iy] = texCds[3][iy] =
        flipInfo.coords[1][iy] + polSize.height / (tSz[iy] * (flipInfo.coords[3][iy] - flipInfo.coords[1][iy]));
} // no copy in retail (inlined)

/*
 * GetLBFrameSize
 *
 * Position and size of the bottom-left frame part (it runs up the left edge).
 */
void GetLBFrameSize(math::VEC2* pPt, Size* pSize, const math::VEC2& basePt, const Size& winSize,
                    const WindowFrameSize& frameSize) {
    *pPt = math::VEC2(basePt.x, basePt.y + frameSize.t);

    pSize->width = frameSize.l;
    pSize->height = winSize.height - frameSize.t;
} // no copy in retail (inlined)

/*
 * GetLBTexCoord
 */
void GetLBTexCoord(math::VEC2* texCds, const Size& polSize, const Size& texSize, u8 textureFlip) {
    const TextureFlipInfo& flipInfo = GetTexutreFlipInfo(textureFlip);
    int ix = flipInfo.idx[0];
    int iy = flipInfo.idx[1];
    const math::VEC2 tSz(texSize.width, texSize.height);

    texCds[2][ix] = texCds[0][ix] = flipInfo.coords[2][ix];
    texCds[2][iy] = texCds[3][iy] = flipInfo.coords[2][iy];

    texCds[1][ix] = texCds[3][ix] =
        flipInfo.coords[2][ix] + polSize.width / (tSz[ix] * (flipInfo.coords[3][ix] - flipInfo.coords[2][ix]));

    texCds[1][iy] = texCds[0][iy] =
        flipInfo.coords[2][iy] + polSize.height / (tSz[iy] * (flipInfo.coords[0][iy] - flipInfo.coords[2][iy]));
} // no copy in retail (inlined)

/*
 * GetRBFrameSize
 *
 * Position and size of the bottom-right frame part (it runs along the bottom edge).
 */
void GetRBFrameSize(math::VEC2* pPt, Size* pSize, const math::VEC2& basePt, const Size& winSize,
                    const WindowFrameSize& frameSize) {
    *pPt = math::VEC2(basePt.x + frameSize.l, basePt.y + winSize.height - frameSize.b);

    pSize->width = winSize.width - frameSize.l;
    pSize->height = frameSize.b;
} // no copy in retail (inlined)

/*
 * GetRBTexCoord
 */
void GetRBTexCoord(math::VEC2* texCds, const Size& polSize, const Size& texSize, u8 textureFlip) {
    const TextureFlipInfo& flipInfo = GetTexutreFlipInfo(textureFlip);
    int ix = flipInfo.idx[0];
    int iy = flipInfo.idx[1];
    const math::VEC2 tSz(texSize.width, texSize.height);

    texCds[3][ix] = texCds[1][ix] = flipInfo.coords[3][ix];
    texCds[3][iy] = texCds[2][iy] = flipInfo.coords[3][iy];

    texCds[0][ix] = texCds[2][ix] =
        flipInfo.coords[3][ix] + polSize.width / (tSz[ix] * (flipInfo.coords[2][ix] - flipInfo.coords[3][ix]));

    texCds[0][iy] = texCds[1][iy] =
        flipInfo.coords[3][iy] + polSize.height / (tSz[iy] * (flipInfo.coords[1][iy] - flipInfo.coords[3][iy]));
} // no copy in retail (inlined)

/*
 * SetFrameTexOrigin
 *
 * Writes one corner's texture coordinate to its two neighbours as well. The corner bits
 * select the horizontal (1) and vertical (2) neighbours.
 */
inline void SetFrameTexOrigin(math::VEC2* texCds, int corner, int ix, int iy, u8 originX, u8 originY) {
    texCds[corner][ix] = texCds[corner ^ 2][ix] = originX;
    texCds[corner][iy] = texCds[corner ^ 1][iy] = originY;
} // no copy in retail (inlined)

/*
 * SetFrameTexExtent
 *
 * Writes the far corner's texture coordinate from the one the part covers.
 */
inline void SetFrameTexExtent(math::VEC2* texCds, const Size& polSize, const Size& texSize,
                              const TextureFlipInfo& flipInfo, int corner, int ix, int iy, u8 originX,
                              u8 originY) {
    const math::VEC2 tSz(texSize.width, texSize.height);

    texCds[corner ^ 3][ix] = texCds[corner ^ 1][ix] =
        originX + polSize.width / ((flipInfo.coords[corner ^ 1][ix] - originX) * tSz[ix]);

    texCds[corner ^ 3][iy] = texCds[corner ^ 2][iy] =
        originY + polSize.height / ((flipInfo.coords[corner ^ 2][iy] - originY) * tSz[iy]);
} // no copy in retail (inlined)

} // namespace

namespace nw4hbm {
namespace lyt {

const ut::detail::RuntimeTypeInfo Window::typeInfo(&Pane::typeInfo);

/*
 * Window::Window
 *
 * Builds the window from its data block: content colours, texture coordinates and material,
 * then one material per frame part.
 */
Window::Window(const res::Window* pBlock, const ResBlockSet& resBlockSet) : Pane(pBlock) {
    mContentInflation = pBlock->inflation;

    const u32* const matOffsTbl =
        detail::ConvertOffsToPtr<u32>(resBlockSet.pMaterialList, sizeof *resBlockSet.pMaterialList);

    const res::WindowContent* pResContent =
        detail::ConvertOffsToPtr<res::WindowContent>(pBlock, pBlock->contentOffset);

    for (int i = 0; i < VERTEXCOLOR_MAX; ++i) {
        mContent.vtxColors[i] = pResContent->vtxCols[i];
    }

    if (pResContent->texCoordNum) {
        u8 texCoordNum = ut::Min<u8>(pResContent->texCoordNum, 8);
        mContent.texCoordAry.Reserve(texCoordNum);

        if (!mContent.texCoordAry.IsEmpty()) {
            mContent.texCoordAry.Copy(pResContent + 1, texCoordNum);
        }
    }

    if (void* pMemMaterial = Layout::AllocMemory(sizeof(Material))) {
        const res::Material* pResMaterial = detail::ConvertOffsToPtr<res::Material>(
            resBlockSet.pMaterialList, matOffsTbl[pResContent->materialIdx]);

        mpMaterial = new (pMemMaterial) Material(pResMaterial, resBlockSet);
    }

    mFrameNum = 0;
    mFrames = NULL;

    if (pBlock->frameNum) {
        mFrames = static_cast<Frame*>(Layout::AllocMemory(sizeof *mFrames * pBlock->frameNum));
        if (mFrames) {
            mFrameNum = pBlock->frameNum;

            const u32* frameOffsetTable = detail::ConvertOffsToPtr<u32>(pBlock, pBlock->frameOffsetTableOffset);

            for (int i = 0; i < mFrameNum; ++i) {
                const res::WindowFrame* pResWindowFrame =
                    detail::ConvertOffsToPtr<res::WindowFrame>(pBlock, frameOffsetTable[i]);

                mFrames[i].textureFlip = pResWindowFrame->textureFlip;
                mFrames[i].pMaterial = NULL;

                if (void* pMemMaterial = Layout::AllocMemory(sizeof(Material))) {
                    const res::Material* pResMaterial = detail::ConvertOffsToPtr<res::Material>(
                        resBlockSet.pMaterialList, matOffsTbl[pResWindowFrame->materialIdx]);

                    mFrames[i].pMaterial = new (pMemMaterial) Material(pResMaterial, resBlockSet);
                }
            }
        }
    }
} // 0x803C9AE0

/*
 * Window::~Window
 */
Window::~Window() {
    if (mFrames) {
        for (int i = 0; i < mFrameNum; ++i) {
            mFrames[i].pMaterial->~Material();
            Layout::FreeMemory(mFrames[i].pMaterial);
        }

        Layout::FreeMemory(mFrames);
    }

    if (mpMaterial && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
        mpMaterial = NULL;
    }

    mContent.texCoordAry.Free();
} // 0x803C9CF0

/*
 * Window::FindMaterialByName
 *
 * Looks for the material in the content, then the frame parts, then (if asked) the
 * descendants.
 */
Material* Window::FindMaterialByName(const char* findName, bool bRecursive) {
    if (mpMaterial && detail::EqualsMaterialName(mpMaterial->GetName(), findName)) {
        return mpMaterial;
    }

    for (int i = 0; i < mFrameNum; ++i) {
        if (detail::EqualsMaterialName(mFrames[i].pMaterial->GetName(), findName)) {
            return mFrames[i].pMaterial;
        }
    }

    if (bRecursive) {
        NW4HBM_RANGE_FOR(it, mChildList) {
            if (Material* pMat = it->FindMaterialByName(findName, true)) {
                return pMat;
            }
        }
    }

    return NULL;
} // 0x803C9E40

/*
 * Window::FindAnimationLink
 */
AnimationLink* Window::FindAnimationLink(AnimTransform* pAnimTrans) {
    if (AnimationLink* ret = Pane::FindAnimationLink(pAnimTrans)) {
        return ret;
    }

    for (int i = 0; i < mFrameNum; ++i) {
        if (AnimationLink* ret = mFrames[i].pMaterial->FindAnimationLink(pAnimTrans)) {
            return ret;
        }
    }

    return NULL;
} // 0x803C9F40

/*
 * Window::SetAnimationEnable
 */
void Window::SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable, bool bRecursive) {
    for (int i = 0; i < mFrameNum; ++i) {
        mFrames[i].pMaterial->SetAnimationEnable(pAnimTrans, bEnable);
    }

    Pane::SetAnimationEnable(pAnimTrans, bEnable, bRecursive);
} // 0x803C9FF0

/*
 * Window::GetVtxColor
 */
ut::Color Window::GetVtxColor(u32 idx) const {
    return mContent.vtxColors[idx];
} // 0x803CA090

/*
 * Window::SetVtxColor
 */
void Window::SetVtxColor(u32 idx, ut::Color value) {
    mContent.vtxColors[idx] = value;
} // 0x803CA0B0

/*
 * Window::GetVtxColorElement
 */
u8 Window::GetVtxColorElement(u32 idx) const {
    return detail::GetVtxColorElement(mContent.vtxColors, idx);
} // 0x803CA0E0

/*
 * Window::SetVtxColorElement
 */
void Window::SetVtxColorElement(u32 idx, u8 value) {
    detail::SetVtxColorElement(mContent.vtxColors, idx, value);
} // 0x803CA100

/*
 * Window::DrawSelf
 *
 * Draws the content, then the frame in the form its part count gives.
 */
void Window::DrawSelf(const DrawInfo& drawInfo) {
    LoadMtx(drawInfo);

    WindowFrameSize frameSize = GetFrameSize(mFrameNum, mFrames);
    math::VEC2 basePt = GetVtxPos();

    DrawContent(basePt, frameSize, mGlbAlpha);

    switch (mFrameNum) {
    case 1:
        DrawFrame(basePt, *mFrames, frameSize, mGlbAlpha);
        break;

    case 4:
        DrawFrame4(basePt, mFrames, frameSize, mGlbAlpha);
        break;

    case 8:
        DrawFrame8(basePt, mFrames, frameSize, mGlbAlpha);
        break;
    }
} // 0x803CA120

/*
 * Window::AnimateSelf
 *
 * Animates the pane, then the frame materials (of an invisible window only when `option` bit 0
 * is clear).
 */
void Window::AnimateSelf(u32 option) {
    Pane::AnimateSelf(option);

    if (detail::TestBit(mFlag, BIT_VISIBLE) || !(option & 1)) {
        for (int i = 0; i < mFrameNum; ++i) {
            mFrames[i].pMaterial->Animate();
        }
    }
} // 0x803CA260

/*
 * Window::UnbindAnimationSelf
 */
void Window::UnbindAnimationSelf(AnimTransform* pAnimTrans) {
    for (int i = 0; i < mFrameNum; ++i) {
        mFrames[i].pMaterial->UnbindAnimation(pAnimTrans);
    }

    Pane::UnbindAnimationSelf(pAnimTrans);
} // 0x803CA2F0

/*
 * Window::DrawContent
 *
 * Draws the content quad inside the frame, grown by the content inflation.
 */
void Window::DrawContent(const math::VEC2& basePt, const WindowFrameSize& frameSize, u8 alpha) {
    bool bUseVtxCol = mpMaterial->SetupGX(detail::IsModulateVertexColor(mContent.vtxColors, alpha), alpha);

    detail::SetVertexFormat(bUseVtxCol, mContent.texCoordAry.GetSize());

    detail::DrawQuad(math::VEC2(basePt.x + frameSize.l - mContentInflation.l,
                                basePt.y + frameSize.t - mContentInflation.t),
                     Size(mSize.width - frameSize.l + mContentInflation.l - frameSize.r + mContentInflation.r,
                          mSize.height - frameSize.t + mContentInflation.t - frameSize.b + mContentInflation.b),
                     mContent.texCoordAry.GetSize(), mContent.texCoordAry.GetArray(),
                     bUseVtxCol ? mContent.vtxColors : NULL, alpha);
} // 0x803CA380

/*
 * Window::DrawFrame
 *
 * Draws a one-part frame: its one texture at all four corners, flipped to fit each.
 *
 * The top-left part is set up in two steps, origin then extent: its origin texture coordinate is
 * also the origin of the parts beside it, so it is read once and written to the neighbouring
 * corners before the covered extent is computed. The three other parts are plain quads.
 */
void Window::DrawFrame(const math::VEC2& basePt, const Frame& frame, const WindowFrameSize& frameSize, u8 alpha) {
    const u8 texCoordNum = 1;

    bool bUseVtxCol = frame.pMaterial->SetupGX(detail::IsModulateVertexColor(NULL, alpha), alpha);
    detail::SetVertexFormat(bUseVtxCol, texCoordNum);

    Size texSize = detail::GetTextureSize(frame.pMaterial, 0);
    ut::Color vtxColors[VERTEXCOLOR_MAX];
    detail::TexCoords texCds[1];
    math::VEC2 polPt;
    Size polSize;

#define DRAW_FRAME_QUAD(corner, textureFlip)                                                       \
    Get##corner##FrameSize(&polPt, &polSize, basePt, mSize, frameSize);                            \
    Get##corner##TexCoord(*texCds, polSize, texSize, textureFlip);                                 \
    detail::DrawQuad(polPt, polSize, texCoordNum, texCds, bUseVtxCol ? vtxColors : NULL, alpha);

    GetLTFrameSize(&polPt, &polSize, basePt, mSize, frameSize);
    const TextureFlipInfo& flipInfo = GetTexutreFlipInfo(0); // no texture flip
    int iy = flipInfo.idx[1];
    int ix = flipInfo.idx[0];
    const u8 originX = flipInfo.coords[0][ix];
    const u8 originY = flipInfo.coords[0][iy];
    SetFrameTexOrigin(*texCds, 0, ix, iy, originX, originY);
    SetFrameTexExtent(*texCds, polSize, texSize, flipInfo, 0, ix, iy, originX, originY);
    detail::DrawQuad(polPt, polSize, texCoordNum, texCds, bUseVtxCol ? vtxColors : NULL, alpha);

    DRAW_FRAME_QUAD(RT, 1)
    DRAW_FRAME_QUAD(RB, 4)
    DRAW_FRAME_QUAD(LB, 2)

#undef DRAW_FRAME_QUAD
} // 0x803CA490

/*
 * Window::DrawFrame4
 *
 * Draws a four-part frame: one part per corner, each with its own material and flip.
 */
void Window::DrawFrame4(const math::VEC2& basePt, const Frame* frames, const WindowFrameSize& frameSize, u8 alpha) {
    const u8 texCoordNum = 1;

    ut::Color vtxColors[VERTEXCOLOR_MAX];
    detail::TexCoords texCds[1];
    math::VEC2 polPt;
    Size polSize;
    bool bModVtxCol = detail::IsModulateVertexColor(NULL, alpha);

#define DRAW_FRAME_QUAD(corner, frameIdx)                                                                    \
    {                                                                                                        \
        bool bUseVtxCol = frames[(frameIdx)].pMaterial->SetupGX(bModVtxCol, alpha);                          \
                                                                                                             \
        Get##corner##FrameSize(&polPt, &polSize, basePt, mSize, frameSize);                                  \
        Get##corner##TexCoord(*texCds, polSize, detail::GetTextureSize(frames[(frameIdx)].pMaterial, 0),     \
                              frames[(frameIdx)].textureFlip);                                               \
                                                                                                             \
        detail::SetVertexFormat(bUseVtxCol, texCoordNum);                                                    \
        detail::DrawQuad(polPt, polSize, texCoordNum, texCds, bUseVtxCol ? vtxColors : NULL, alpha);         \
    }

    DRAW_FRAME_QUAD(LT, 0)
    DRAW_FRAME_QUAD(RT, 1)
    DRAW_FRAME_QUAD(RB, 3)
    DRAW_FRAME_QUAD(LB, 2)

#undef DRAW_FRAME_QUAD
} // 0x803CAA70

/*
 * Window::DrawFrame8
 *
 * Draws an eight-part frame: the four corners and the four edges between them, each with its
 * own material and flip; an edge takes the texture coordinates of the corner before it.
 */
void Window::DrawFrame8(const math::VEC2& basePt, const Frame* frames, const WindowFrameSize& frameSize, u8 alpha) {
    const u8 texCoordNum = 1;

    ut::Color vtxColors[VERTEXCOLOR_MAX];
    detail::TexCoords texCds[1];
    Size polSize;
    bool bModVtxCol = detail::IsModulateVertexColor(NULL, alpha);

#define DRAW_FRAME_QUAD(corner, frameIdx, size, pt)                                                          \
    {                                                                                                        \
        bool bUseVtxCol = frames[(frameIdx)].pMaterial->SetupGX(bModVtxCol, alpha);                          \
        polSize = (size);                                                                                    \
                                                                                                             \
        Get##corner##TexCoord(*texCds, polSize, detail::GetTextureSize(frames[(frameIdx)].pMaterial, 0),     \
                              frames[(frameIdx)].textureFlip);                                               \
                                                                                                             \
        detail::SetVertexFormat(bUseVtxCol, texCoordNum);                                                    \
        detail::DrawQuad(pt, polSize, texCoordNum, texCds, bUseVtxCol ? vtxColors : NULL, alpha);            \
    }

    DRAW_FRAME_QUAD(LT, 0, Size(frameSize.l, frameSize.t), basePt)
    DRAW_FRAME_QUAD(LT, 6, Size(mSize.width - frameSize.l - frameSize.r, frameSize.t),
                    math::VEC2(basePt.x + frameSize.l, basePt.y))
    DRAW_FRAME_QUAD(RT, 1, Size(frameSize.r, frameSize.t),
                    math::VEC2(basePt.x + mSize.width - frameSize.r, basePt.y))
    DRAW_FRAME_QUAD(RT, 5, Size(frameSize.r, mSize.height - frameSize.t - frameSize.b),
                    math::VEC2(basePt.x + mSize.width - frameSize.r, basePt.y + frameSize.t))
    DRAW_FRAME_QUAD(RB, 3, Size(frameSize.r, frameSize.b),
                    math::VEC2(basePt.x + mSize.width - frameSize.r, basePt.y + mSize.height - frameSize.b))
    DRAW_FRAME_QUAD(RB, 7, Size(mSize.width - frameSize.l - frameSize.r, frameSize.b),
                    math::VEC2(basePt.x + frameSize.l, basePt.y + mSize.height - frameSize.b))
    DRAW_FRAME_QUAD(LB, 2, Size(frameSize.l, frameSize.b),
                    math::VEC2(basePt.x, basePt.y + mSize.height - frameSize.b))
    DRAW_FRAME_QUAD(LB, 4, Size(frameSize.l, mSize.height - frameSize.t - frameSize.b),
                    math::VEC2(basePt.x, basePt.y + frameSize.t))

#undef DRAW_FRAME_QUAD
} // 0x803CB140

/*
 * Window::GetFrameSize
 *
 * The frame thickness from the texture sizes: one part's texture gives every side; with four
 * or eight parts the top-left corner gives the left and top, the bottom-right corner the right
 * and bottom.
 */
WindowFrameSize Window::GetFrameSize(u8 frameNum, const Frame* frames) {
    WindowFrameSize ret = {};

    switch (frameNum) {
    case 1: {
        Size texSize = detail::GetTextureSize(frames->pMaterial, 0);
        ret.l = texSize.width;
        ret.t = texSize.height;

        ret.r = texSize.width;
        ret.b = texSize.height;
        break;
    }

    case 4:
    case 8: {
        Size texSize = detail::GetTextureSize(frames[0].pMaterial, 0);
        ret.l = texSize.width;
        ret.t = texSize.height;

        texSize = detail::GetTextureSize(frames[3].pMaterial, 0);
        ret.r = texSize.width;
        ret.b = texSize.height;
        break;
    }
    }

    return ret;
} // 0x803CBE70

/*
 * Window::GetFrameMaterial
 */
Material* Window::GetFrameMaterial(u32 frameIdx) const {
    if (frameIdx >= mFrameNum) {
        return NULL;
    }

    return mFrames[frameIdx].pMaterial;
} // 0x803CBF70

/*
 * Window::GetContentMaterial
 */
Material* Window::GetContentMaterial() const {
    return GetMaterial();
} // 0x803CBFA0

} // namespace lyt
} // namespace nw4hbm
