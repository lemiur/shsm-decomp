// Reference: zeldaret/tp libs/revolution/src/gx/GXTransform.c, doldecomp/ogws GXTransform.c (asm helpers),
// SMGCommunity/Petari src/RVL_SDK/gx/GXTransform.c and koopthekoopa/wii-ipl libs/RVL_SDK/src/gx/GXTransform.c (pragma)
#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

// Functions that expand the inline-asm matrix helpers are not IR-optimised (SMGCommunity/Petari
// src/RVL_SDK/gx/GXTransform.c, koopthekoopa/wii-ipl libs/RVL_SDK/src/gx/GXTransform.c).
#pragma optimizewithasm off

// GXProject: transforms a model-space point to screen space with the given position matrix, projection and viewport.
// Not in the retail binary (dead-stripped).
void GXProject(f32 x, f32 y, f32 z, const Mtx mtx, const f32* pm, const f32* vp, f32* sx, f32* sy, f32* sz) {
    Vec peye;
    f32 xc;
    f32 yc;
    f32 zc;
    f32 wc;

    ASSERTMSG(pm && vp && sx && sy && sz, "GXGet*: invalid null pointer");

    peye.x = mtx[0][3] + ((mtx[0][2] * z) + ((mtx[0][0] * x) + (mtx[0][1] * y)));
    peye.y = mtx[1][3] + ((mtx[1][2] * z) + ((mtx[1][0] * x) + (mtx[1][1] * y)));
    peye.z = mtx[2][3] + ((mtx[2][2] * z) + ((mtx[2][0] * x) + (mtx[2][1] * y)));
    if (pm[0] == 0.0f) {
        xc = (peye.x * pm[1]) + (peye.z * pm[2]);
        yc = (peye.y * pm[3]) + (peye.z * pm[4]);
        zc = pm[6] + (peye.z * pm[5]);
        wc = 1.0f / -peye.z;
    } else {
        xc = pm[2] + (peye.x * pm[1]);
        yc = pm[4] + (peye.y * pm[3]);
        zc = pm[6] + (peye.z * pm[5]);
        wc = 1.0f;
    }
    *sx = (vp[2] / 2.0f) + (vp[0] + (wc * (xc * vp[2] / 2.0f)));
    *sy = (vp[3] / 2.0f) + (vp[1] + (wc * (-yc * vp[3] / 2.0f)));
    *sz = vp[5] + (wc * (zc * (vp[5] - vp[4])));
}

// Graphics quantization register 0 (no scaling) used by the paired-single loads and stores below.
#define qr0 0

// SDK inline-asm helper: copies the 6 projection floats to the write-gather pipe.
static void WriteProjPS(const register f32 proj[6], register volatile void* dest) { // volatile: write-gather pipe port
    register f32 p01, p23, p45;

#ifdef __MWERKS__
    asm {
        psq_l  p01,  0(proj), 0, qr0
        psq_l  p23,  8(proj), 0, qr0
        psq_l  p45, 16(proj), 0, qr0
        psq_st p01,  0(dest), 0, qr0
        psq_st p23,  0(dest), 0, qr0
        psq_st p45,  0(dest), 0, qr0
    }
#endif
}

// SDK inline-asm helper: copies 6 floats with three paired-single moves.
static void Copy6Floats(const register f32 src[6], register volatile f32* dest) { // volatile: write-gather pipe port
    register f32 ps01, ps23, ps45;

    asm {
        psq_l  ps01,  0(src), 0, qr0
        psq_l  ps23,  8(src), 0, qr0
        psq_l  ps45, 16(src), 0, qr0
        psq_st ps01,  0(dest), 0, qr0
        psq_st ps23,  8(dest), 0, qr0
        psq_st ps45, 16(dest), 0, qr0
    }
}

// __GXSetProjection: writes the cached projection matrix and type to the XF projection registers.
// 0x8023E260
void __GXSetProjection(void) {
    u32 reg = 0x00061020;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
#if DEBUG
    GX_WRITE_XF_REG_F(32, __GXData->projMtx[0]);
    GX_WRITE_XF_REG_F(33, __GXData->projMtx[1]);
    GX_WRITE_XF_REG_F(34, __GXData->projMtx[2]);
    GX_WRITE_XF_REG_F(35, __GXData->projMtx[3]);
    GX_WRITE_XF_REG_F(36, __GXData->projMtx[4]);
    GX_WRITE_XF_REG_F(37, __GXData->projMtx[5]);
    GX_WRITE_XF_REG_2(38, __GXData->projType);
#else
    WriteProjPS(__GXData->projMtx, &GXWGFifo);
    GX_WRITE_U32(__GXData->projType);
#endif
}

// GXSetProjection: caches a 4x4 projection matrix (perspective or orthographic) and flags it dirty.
// 0x8023E2B0
void GXSetProjection(const Mtx44 mtx, GXProjectionType type) {
    CHECK_GXBEGIN("GXSetProjection");

    __GXData->projType = type;
    __GXData->projMtx[0] = mtx[0][0];
    __GXData->projMtx[2] = mtx[1][1];
    __GXData->projMtx[4] = mtx[2][2];
    __GXData->projMtx[5] = mtx[2][3];
    if (type == GX_ORTHOGRAPHIC) {
        __GXData->projMtx[1] = mtx[0][3];
        __GXData->projMtx[3] = mtx[1][3];
    } else {
        __GXData->projMtx[1] = mtx[0][2];
        __GXData->projMtx[3] = mtx[1][2];
    }

    __GXData->dirtyState |= 0x8000000;
}

// GXSetProjectionv: caches a projection given as type + 6 parameters and flags it dirty.
// 0x8023E320
void GXSetProjectionv(const f32* ptr) {
    CHECK_GXBEGIN("GXSetProjectionv");

    __GXData->projType = ptr[0] == 0.0f ? GX_PERSPECTIVE : GX_ORTHOGRAPHIC;

#if DEBUG
    __GXData->projMtx[0] = ptr[1];
    __GXData->projMtx[1] = ptr[2];
    __GXData->projMtx[2] = ptr[3];
    __GXData->projMtx[3] = ptr[4];
    __GXData->projMtx[4] = ptr[5];
    __GXData->projMtx[5] = ptr[6];
#else
    Copy6Floats(&ptr[1], __GXData->projMtx);
#endif

    __GXData->dirtyState |= 0x8000000;
}

// SDK inline-asm helper: copies the cached projection floats out of __GXData.
static void LoadProjPS(register f32* dest) {
    register f32 ps_0, ps_1, ps_2;
    register GXData* src;

    asm {
        lwz    src, __GXData
        psq_l  ps_0,  GXData.projMtx(src), 0, qr0
        psq_l  ps_1,  GXData.projMtx+8(src), 0, qr0
        psq_l  ps_2,  GXData.projMtx+16(src), 0, qr0
        psq_st ps_0,  0(dest), 0, qr0
        psq_st ps_1,  8(dest), 0, qr0
        psq_st ps_2, 16(dest), 0, qr0
    }
}

// GXGetProjectionv: returns the cached projection as type + 6 parameters.
// 0x8023E370
void GXGetProjectionv(f32* ptr) {
    ASSERTMSG(ptr, "GXGet*: invalid null pointer");

    ptr[0] = (u32)__GXData->projType != GX_PERSPECTIVE ? 1.0f : 0.0f;

#if DEBUG
    ptr[1] = __GXData->projMtx[0];
    ptr[2] = __GXData->projMtx[1];
    ptr[3] = __GXData->projMtx[2];
    ptr[4] = __GXData->projMtx[3];
    ptr[5] = __GXData->projMtx[4];
    ptr[6] = __GXData->projMtx[5];
#else
    LoadProjPS(&ptr[1]);
#endif
}

// SDK inline-asm helpers: stream a matrix to the write-gather pipe with paired-single moves.
// 4x3 matrix: six paired loads, six paired stores.
static void WriteMTXPS4x3(const register f32 mtx[3][4], register volatile f32* dest) { // volatile: write-gather pipe port
    register f32 a00_a01;
    register f32 a02_a03;
    register f32 a10_a11;
    register f32 a12_a13;
    register f32 a20_a21;
    register f32 a22_a23;

    asm {
        psq_l a00_a01, 0x00(mtx), 0, qr0
        psq_l a02_a03, 0x08(mtx), 0, qr0
        psq_l a10_a11, 0x10(mtx), 0, qr0
        psq_l a12_a13, 0x18(mtx), 0, qr0
        psq_l a20_a21, 0x20(mtx), 0, qr0
        psq_l a22_a23, 0x28(mtx), 0, qr0
        psq_st a00_a01, 0(dest), 0, qr0
        psq_st a02_a03, 0(dest), 0, qr0
        psq_st a10_a11, 0(dest), 0, qr0
        psq_st a12_a13, 0(dest), 0, qr0
        psq_st a20_a21, 0(dest), 0, qr0
        psq_st a22_a23, 0(dest), 0, qr0
    }
}

// 3x3 part of a 3x4 matrix: pairs for columns 0-1, scalar for column 2.
static void WriteMTXPS3x3from3x4(const register f32 mtx[3][4], register volatile f32* dest) { // volatile: write-gather pipe port
    register f32 a00_a01;
    register f32 a02_a03;
    register f32 a10_a11;
    register f32 a12_a13;
    register f32 a20_a21;
    register f32 a22_a23;

    asm {
        psq_l  a00_a01, 0x00(mtx), 0, qr0
        lfs    a02_a03, 0x08(mtx)
        psq_l  a10_a11, 0x10(mtx), 0, qr0
        lfs    a12_a13, 0x18(mtx)
        psq_l  a20_a21, 0x20(mtx), 0, qr0
        lfs    a22_a23, 0x28(mtx)
        psq_st a00_a01, 0(dest), 0, qr0
        stfs   a02_a03, 0(dest)
        psq_st a10_a11, 0(dest), 0, qr0
        stfs   a12_a13, 0(dest)
        psq_st a20_a21, 0(dest), 0, qr0
        stfs   a22_a23, 0(dest)
    }
}

// Packed 3x3 matrix: four pairs and one scalar.
static void WriteMTXPS3x3(const register f32 mtx[3][3], register volatile f32* dest) { // volatile: write-gather pipe port
    register f32 a00_a01;
    register f32 a02_a10;
    register f32 a11_a12;
    register f32 a20_a21;
    register f32 a22_nnn;

    asm {
        psq_l a00_a01, 0x00(mtx), 0, qr0
        psq_l a02_a10, 0x08(mtx), 0, qr0
        psq_l a11_a12, 0x10(mtx), 0, qr0
        psq_l a20_a21, 0x18(mtx), 0, qr0
        lfs   a22_nnn, 0x20(mtx)
        psq_st a00_a01, 0(dest), 0, qr0
        psq_st a02_a10, 0(dest), 0, qr0
        psq_st a11_a12, 0(dest), 0, qr0
        psq_st a20_a21, 0(dest), 0, qr0
        stfs   a22_nnn, 0(dest)
    }
}

// 2x4 matrix: four paired loads and stores.
static void WriteMTXPS4x2(const register f32 mtx[2][4], register volatile f32* dest) { // volatile: write-gather pipe port
    register f32 a00_a01;
    register f32 a02_a03;
    register f32 a10_a11;
    register f32 a12_a13;

    asm {
        psq_l a00_a01, 0x00(mtx), 0, qr0
        psq_l a02_a03, 0x08(mtx), 0, qr0
        psq_l a10_a11, 0x10(mtx), 0, qr0
        psq_l a12_a13, 0x18(mtx), 0, qr0
        psq_st a00_a01, 0(dest), 0, qr0
        psq_st a02_a03, 0(dest), 0, qr0
        psq_st a10_a11, 0(dest), 0, qr0
        psq_st a12_a13, 0(dest), 0, qr0
    }
}

#define GX_WRITE_MTX_ELEM(addr, value) \
do { \
    f32 xfData = (value); \
    GX_WRITE_F32(value); \
    VERIF_MTXLIGHT((addr), *(u32 *)&xfData); \
} while (0)

// GXLoadPosMtxImm: loads a 3x4 position matrix into XF matrix memory slot id.
// 0x8023E3B0
void GXLoadPosMtxImm(const Mtx mtx, u32 id) {
    u32 addr;

    CHECK_GXBEGIN("GXLoadPosMtxImm");

    addr = id * 4;
    GX_XF_LOAD_REGS(12 - 1, addr);
#if DEBUG
    GX_WRITE_MTX_ELEM(addr, mtx[0][0]);
    GX_WRITE_MTX_ELEM(addr + 1, mtx[0][1]);
    GX_WRITE_MTX_ELEM(addr + 2, mtx[0][2]);
    GX_WRITE_MTX_ELEM(addr + 3, mtx[0][3]);
    GX_WRITE_MTX_ELEM(addr + 4, mtx[1][0]);
    GX_WRITE_MTX_ELEM(addr + 5, mtx[1][1]);
    GX_WRITE_MTX_ELEM(addr + 6, mtx[1][2]);
    GX_WRITE_MTX_ELEM(addr + 7, mtx[1][3]);
    GX_WRITE_MTX_ELEM(addr + 8, mtx[2][0]);
    GX_WRITE_MTX_ELEM(addr + 9, mtx[2][1]);
    GX_WRITE_MTX_ELEM(addr + 10, mtx[2][2]);
    GX_WRITE_MTX_ELEM(addr + 11, mtx[2][3]);
#else
    WriteMTXPS4x3(mtx, &GXWGFifo.f32);
#endif
}

// GXLoadPosMtxIndx: loads a position matrix into slot id from the indexed matrix array.
// Not in the retail binary (dead-stripped).
void GXLoadPosMtxIndx(u16 mtx_indx, u32 id) {
    u32 offset;
    u32 reg;

    CHECK_GXBEGIN("GXLoadPosMtxIndx");
    offset = id * 4;
    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 12, 0, offset);
    SET_REG_FIELD(__LINE__, reg, 4, 12, 11);
    SET_REG_FIELD(__LINE__, reg, 16, 16, mtx_indx);
    GX_WRITE_U8(0x20);
    GX_WRITE_U32(reg);
#if DEBUG
    __GXShadowIndexState(4, reg);
#endif
}

// GXLoadNrmMtxImm: loads the 3x3 part of a 3x4 matrix as normal matrix slot id.
// 0x8023E400
void GXLoadNrmMtxImm(const Mtx mtx, u32 id) {
    u32 addr;

    CHECK_GXBEGIN("GXLoadNrmMtxImm");

    addr = id * 3 + 0x400;
    GX_XF_LOAD_REGS(9 - 1, addr);
#if DEBUG
    GX_WRITE_MTX_ELEM(addr, mtx[0][0]);
    GX_WRITE_MTX_ELEM(addr + 1, mtx[0][1]);
    GX_WRITE_MTX_ELEM(addr + 2, mtx[0][2]);
    GX_WRITE_MTX_ELEM(addr + 3, mtx[1][0]);
    GX_WRITE_MTX_ELEM(addr + 4, mtx[1][1]);
    GX_WRITE_MTX_ELEM(addr + 5, mtx[1][2]);
    GX_WRITE_MTX_ELEM(addr + 6, mtx[2][0]);
    GX_WRITE_MTX_ELEM(addr + 7, mtx[2][1]);
    GX_WRITE_MTX_ELEM(addr + 8, mtx[2][2]);
#else
    WriteMTXPS3x3from3x4(mtx, &GXWGFifo.f32);
#endif
}

// GXLoadNrmMtxImm3x3: loads a 3x3 normal matrix into slot id.
// Not in the retail binary (dead-stripped).
void GXLoadNrmMtxImm3x3(const f32 mtx[3][3], u32 id) {
    u32 addr;

    CHECK_GXBEGIN("GXLoadNrmMtxImm3x3");

    addr = id * 3 + 0x400;
    GX_XF_LOAD_REGS(9 - 1, addr);
#if DEBUG
    GX_WRITE_MTX_ELEM(addr, mtx[0][0]);
    GX_WRITE_MTX_ELEM(addr + 1, mtx[0][1]);
    GX_WRITE_MTX_ELEM(addr + 2, mtx[0][2]);
    GX_WRITE_MTX_ELEM(addr + 3, mtx[1][0]);
    GX_WRITE_MTX_ELEM(addr + 4, mtx[1][1]);
    GX_WRITE_MTX_ELEM(addr + 5, mtx[1][2]);
    GX_WRITE_MTX_ELEM(addr + 6, mtx[2][0]);
    GX_WRITE_MTX_ELEM(addr + 7, mtx[2][1]);
    GX_WRITE_MTX_ELEM(addr + 8, mtx[2][2]);
#else
    WriteMTXPS3x3(mtx, &GXWGFifo.f32);
#endif
}

// GXLoadNrmMtxIndx3x3: loads a normal matrix into slot id from the indexed matrix array.
// Not in the retail binary (dead-stripped).
void GXLoadNrmMtxIndx3x3(u16 mtx_indx, u32 id) {
    u32 offset;
    u32 reg;

    CHECK_GXBEGIN("GXLoadNrmMtxIndx3x3");
    offset = id * 3 + 0x400;
    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 12, 0, offset);
    SET_REG_FIELD(__LINE__, reg, 4, 12, 8);
    SET_REG_FIELD(__LINE__, reg, 16, 16, mtx_indx);
    GX_WRITE_U8(0x28);
    GX_WRITE_U32(reg);
#if DEBUG
    __GXShadowIndexState(5, reg);
#endif
}

// GXSetCurrentMtx: selects the current position/normal matrix slot.
// 0x8023E460
void GXSetCurrentMtx(u32 id) {
    CHECK_GXBEGIN("GXSetCurrentMtx");
    SET_REG_FIELD(__LINE__, __GXData->matIdxA, 6, 0, id);
    __GXData->dirtyState |= 0x4000000;
}

// GXLoadTexMtxImm: loads a 2x4 or 3x4 texture matrix into texture matrix slot id.
// Both selections (slot address, word count) stay two-armed branches: the inlined asm helpers keep the function out
// of the IR optimiser (`optimizewithasm off` above), so its conditional-assignment passes never run here.
// 0x8023E480
void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, GXTexMtxType type) {
    u32 addr;
    u32 count;

    CHECK_GXBEGIN("GXLoadTexMtxImm");

    if (id >= GX_PTTEXMTX0) {
        addr = (id - GX_PTTEXMTX0) * 4 + 0x500;
        ASSERTMSG(type == GX_MTX3x4, "GXLoadTexMtx: Invalid matrix type");
    } else {
        addr = id * 4;
    }
    count = (type == GX_MTX2x4) ? 8 : 12;
    GX_XF_LOAD_REGS(count - 1, addr);
#if DEBUG
    GX_WRITE_MTX_ELEM(addr, mtx[0][0]);
    GX_WRITE_MTX_ELEM(addr + 1, mtx[0][1]);
    GX_WRITE_MTX_ELEM(addr + 2, mtx[0][2]);
    GX_WRITE_MTX_ELEM(addr + 3, mtx[0][3]);
    GX_WRITE_MTX_ELEM(addr + 4, mtx[1][0]);
    GX_WRITE_MTX_ELEM(addr + 5, mtx[1][1]);
    GX_WRITE_MTX_ELEM(addr + 6, mtx[1][2]);
    GX_WRITE_MTX_ELEM(addr + 7, mtx[1][3]);
    if (type == GX_MTX3x4) {
        GX_WRITE_MTX_ELEM(addr + 8, mtx[2][0]);
        GX_WRITE_MTX_ELEM(addr + 9, mtx[2][1]);
        GX_WRITE_MTX_ELEM(addr + 10, mtx[2][2]);
        GX_WRITE_MTX_ELEM(addr + 11, mtx[2][3]);
    }
#else
    if (type == GX_MTX3x4) {
        WriteMTXPS4x3(mtx, &GXWGFifo.f32);
    } else {
        WriteMTXPS4x2(mtx, &GXWGFifo.f32);
    }
#endif
}

// GXLoadTexMtxIndx: loads a texture matrix into slot id from the indexed matrix array.
// Not in the retail binary (dead-stripped).
void GXLoadTexMtxIndx(u16 mtx_indx, u32 id, GXTexMtxType type) {
    u32 offset;
    u32 reg;
    u32 count;

    CHECK_GXBEGIN("GXLoadTexMtxIndx");

    if (id >= GX_PTTEXMTX0) {
        offset = (id - GX_PTTEXMTX0) * 4 + 0x500;
        ASSERTMSGLINE(0x337, type == GX_MTX3x4, "GXLoadTexMtx: Invalid matrix type");
    } else {
        offset = id * 4;
    }
    count = (type == GX_MTX2x4) ? 8 : 12;

    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 12, 0, offset);
    SET_REG_FIELD(__LINE__, reg, 4, 12, (count - 1));
    SET_REG_FIELD(__LINE__, reg, 16, 16, mtx_indx);
    GX_WRITE_U8(0x30);
    GX_WRITE_U32(reg);
#if DEBUG
    __GXShadowIndexState(6, reg);
#endif
}

// __GXSetViewport: derives the viewport scale/offset from the cached viewport and Z scale and writes the XF registers.
// 0x8023E540
void __GXSetViewport(void) {
    f32 sx;
    f32 sy;
    f32 sz;
    f32 ox;
    f32 oy;
    f32 oz;
    f32 zmin;
    f32 zmax;
    u32 reg;

    sx = __GXData->vpWd / 2.0f;
    sy = -__GXData->vpHt / 2.0f;
    ox = 342.0f + (__GXData->vpLeft + (__GXData->vpWd / 2.0f));
    oy = 342.0f + (__GXData->vpTop + (__GXData->vpHt / 2.0f));

    zmin = __GXData->vpNearz * __GXData->zScale;
    zmax = __GXData->vpFarz * __GXData->zScale;

    sz = zmax - zmin;
    oz = zmax + __GXData->zOffset;

    reg = 0x5101A;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_XF_REG_F(26, sx);
    GX_WRITE_XF_REG_F(27, sy);
    GX_WRITE_XF_REG_F(28, sz);
    GX_WRITE_XF_REG_F(29, ox);
    GX_WRITE_XF_REG_F(30, oy);
    GX_WRITE_XF_REG_F(31, oz);
}

// GXSetViewportJitter: caches the viewport; the even field is shifted up half a line.
// 0x8023E5D0
void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    CHECK_GXBEGIN("GXSetViewport");  // not the correct function name

    if (field == 0) {
        top -= 0.5f;
    }

    __GXData->vpLeft = left;
    __GXData->vpTop = top;
    __GXData->vpWd = wd;
    __GXData->vpHt = ht;
    __GXData->vpNearz = nearz;
    __GXData->vpFarz = farz;
    
    __GXData->dirtyState |= 0x10000000;
}

// GXSetViewport: caches the viewport rectangle and depth range.
// 0x8023E610
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1);
}

// GXGetViewportv: returns the cached viewport (left, top, width, height, near, far).
// Not in the retail binary (dead-stripped).
void GXGetViewportv(f32* vp) {
    ASSERTMSG(vp, "GXGet*: invalid null pointer");

#if DEBUG
    vp[0] = __GXData->vpLeft;
    vp[1] = __GXData->vpTop;
    vp[2] = __GXData->vpWd;
    vp[3] = __GXData->vpHt;
    vp[4] = __GXData->vpNearz;
    vp[5] = __GXData->vpFarz;
#else
    Copy6Floats(&__GXData->vpLeft, vp);
#endif
}

// GXSetZScaleOffset: sets the depth scale/offset and rewrites the viewport Z registers.
// Not in the retail binary (dead-stripped).
void GXSetZScaleOffset(f32 scale, f32 offset) {
    f32 sz;
    f32 oz;
    f32 zmin;
    f32 zmax;

    CHECK_GXBEGIN("GXSetZScaleOffset");

    oz = offset * 1.6777215e7f;
    __GXData->zOffset = oz;

    sz = (scale * 1.6777215e7f) + 1.0f;
    __GXData->zScale = sz;

    zmin = __GXData->vpNearz * sz;
    zmax = __GXData->vpFarz * sz;
    sz = zmax - zmin;
    oz = oz + zmax;

    GX_WRITE_U8(0x10);
    GX_WRITE_U32(0x1000 + 28);
    GX_WRITE_F32(sz);
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(0x1000 + 31);
    GX_WRITE_F32(oz);

    __GXData->bpSentNot = 1;
}

// GXSetScissor: sets the scissor rectangle in the BP scissor registers.
// 0x8023E640
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    u32 tp;
    u32 lf;
    u32 bm;
    u32 rt;

    CHECK_GXBEGIN("GXSetScissor");
    ASSERTMSG(left < 1706, "GXSetScissor: Left origin > 1708");
    ASSERTMSG(top < 1706, "GXSetScissor: top origin > 1708");
    ASSERTMSG(left + wd < 1706, "GXSetScissor: right edge > 1708");
    ASSERTMSG(top + ht < 1706, "GXSetScissor: bottom edge > 1708");

    tp = top + 342;
    lf = left + 342;
    bm = tp + ht - 1;
    rt = lf + wd - 1;

    SET_REG_FIELD(__LINE__, __GXData->suScis0, 11, 0, tp);
    SET_REG_FIELD(__LINE__, __GXData->suScis0, 11, 12, lf);
    SET_REG_FIELD(__LINE__, __GXData->suScis1, 11, 0, bm);
    SET_REG_FIELD(__LINE__, __GXData->suScis1, 11, 12, rt);

    GX_WRITE_RAS_REG(__GXData->suScis0);
    GX_WRITE_RAS_REG(__GXData->suScis1);
    __GXData->bpSentNot = 0;
}

// GXGetScissor: returns the scissor rectangle.
// Not in the retail binary (dead-stripped).
void GXGetScissor(u32* left, u32* top, u32* wd, u32* ht) {
    u32 tp;
    u32 lf;
    u32 bm;
    u32 rt;

    ASSERTMSG(left && top && wd && ht, "GXGet*: invalid null pointer");

    tp = __GXData->suScis0 & 0x7FF;
    lf = (__GXData->suScis0 & 0x7FF000) >> 12;
    bm = __GXData->suScis1 & 0x7FF;
    rt = (__GXData->suScis1 & 0x7FF000) >> 12;

    *left = lf - 342;
    *top = tp - 342;
    *wd = rt - lf + 1;
    *ht = bm - tp + 1;
}

// GXSetScissorBoxOffset: sets the scissor box offset.
// 0x8023E6B0
void GXSetScissorBoxOffset(s32 x_off, s32 y_off) {
    u32 reg = 0;
    u32 hx;
    u32 hy;

    CHECK_GXBEGIN("GXSetScissorBoxOffset");

    ASSERTMSG(!!((u32)(x_off + 342) < 2048), "GXSetScissorBoxOffset: Invalid X offset");
    ASSERTMSG(!!((u32)(y_off + 342) < 2048), "GXSetScissorBoxOffset: Invalid Y offset");

    hx = (u32)(x_off + 342) >> 1;
    hy = (u32)(y_off + 342) >> 1;

    SET_REG_FIELD(__LINE__, reg, 10, 0, hx);
    SET_REG_FIELD(__LINE__, reg, 10, 10, hy);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x59);
    GX_WRITE_RAS_REG(reg);
    __GXData->bpSentNot = 0;
}

// GXSetClipMode: enables or disables clipping in the XF.
// 0x8023E6F0
void GXSetClipMode(GXClipMode mode) {
    CHECK_GXBEGIN("GXSetClipMode");
    GX_WRITE_XF_REG(0x1005, mode, 0);
    __GXData->bpSentNot = 1;
}

// __GXSetMatrixIndex: writes the matrix index registers (A for texmtx 0-3, B for 4-7).
// 0x8023E720
void __GXSetMatrixIndex(GXAttr matIdxAttr) {
    if (matIdxAttr < GX_VA_TEX4MTXIDX) {
        GX_WRITE_SOME_REG4(8, 0x30, __GXData->matIdxA, -12);
        GX_WRITE_XF_REG(0x1018, __GXData->matIdxA, 0);
    } else {
        GX_WRITE_SOME_REG4(8, 0x40, __GXData->matIdxB, -12);
        GX_WRITE_XF_REG(0x1019, __GXData->matIdxB, 0);
    }
    __GXData->bpSentNot = 1;
}
