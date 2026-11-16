// Reference: zeldaret/tp libs/revolution/src/gx/GXLight.c, doldecomp/ogws src/revolution/GX/GXLight.c
#include <math.h>
#include <revolution/gx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

// GXLightObj private data
typedef struct {
    u32 reserved[3];
    u32 Color;
    f32 a[3];
    f32 k[3];
    f32 lpos[3];
    f32 ldir[3];
} __GXLightObjInt_struct;

/*
 * GXInitLightAttn
 *
 * Sets the angle attenuation (a0-a2) and distance attenuation (k0-k2)
 * coefficients of a light.
 */
// 0x8023B940
void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightAttn");
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}


/*
 * GXInitLightAttnA
 *
 * Sets the angle attenuation coefficients of a light.
 */
// 0x8023B960
void GXInitLightAttnA(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightAttnA");
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}


/*
 * GXGetLightAttnA
 *
 * Reads back the angle attenuation coefficients of a light.
 */
void GXGetLightAttnA(const GXLightObj* lt_obj, f32* a0, f32* a1, f32* a2) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXGetLightAttnA");
    *a0 = obj->a[0];
    *a1 = obj->a[1];
    *a2 = obj->a[2];
}


/*
 * GXInitLightAttnK
 *
 * Sets the distance attenuation coefficients of a light.
 */
void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightAttnK");
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}


/*
 * GXGetLightAttnK
 *
 * Reads back the distance attenuation coefficients of a light.
 */
void GXGetLightAttnK(const GXLightObj* lt_obj, f32* k0, f32* k1, f32* k2) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXGetLightAttnK");
    *k0 = obj->k[0];
    *k1 = obj->k[1];
    *k2 = obj->k[2];
}


/*
 * GXInitLightSpot
 *
 * Sets the angle attenuation of a light from a spot cone cutoff (in
 * degrees) and a spot function.
 */
// 0x8023B970
void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func) {
    f32 a0, a1, a2;
    f32 r;
    f32 d;
    f32 cr;
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightSpot");

    if (cutoff <= 0.0f || cutoff > 90.0f) {
        spot_func = GX_SP_OFF;
    }

    r = (3.1415927f * cutoff) / 180.0f;
    cr = cosf(r);

    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0f * cr;
        a1 = 1000.0f;
        a2 = 0.0f;
        break;
    case GX_SP_COS:
        a1 = 1.0f / (1.0f - cr);
        a0 = -cr * a1;
        a2 = 0.0f;
        break;
    case GX_SP_COS2:
        a2 = 1.0f / (1.0f - cr);
        a0 = 0.0f;
        a1 = -cr * a2;
        break;
    case GX_SP_SHARP:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a0 = (cr * (cr - 2.0f)) * d;
        a1 = 2.0f * d;
        a2 = -d;
        break;
    case GX_SP_RING1:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a2 = -4.0f * d;
        a0 = a2 * cr;
        a1 = (4.0f * (1.0f + cr)) * d;
        break;
    case GX_SP_RING2:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a0 = 1.0f - ((2.0f * cr * cr) * d);
        a1 = (4.0f * cr) * d;
        a2 = -2.0f * d;
        break;
    case GX_SP_OFF:
    default:
        a0 = 1.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        break;
    }
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}


/*
 * GXInitLightDistAttn
 *
 * Sets the distance attenuation of a light from the distance at which
 * it falls to a given brightness, and an attenuation function.
 */
// 0x8023BB10
void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func) {
    f32 k0, k1, k2;
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightDistAttn");

    if (ref_dist < 0.0f) {
        dist_func = GX_DA_OFF;
    }
    if (ref_br <= 0.0f || ref_br >= 1.0f) {
        dist_func = GX_DA_OFF;
    }

    switch (dist_func) {
    case GX_DA_GENTLE:
        k0 = 1.0f;
        k1 = (1.0f - ref_br) / (ref_br * ref_dist);
        k2 = 0.0f;
        break;
    case GX_DA_MEDIUM:
        k0 = 1.0f;
        k1 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist);
        k2 = (0.5f * (1.0f - ref_br)) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_STEEP:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = (1.0f - ref_br) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_OFF:
    default:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = 0.0f;
        break;
    }

    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}


/*
 * GXInitLightPos
 *
 * Sets the position of a light.
 */
// 0x8023BBE0
void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightPos");

    obj->lpos[0] = x;
    obj->lpos[1] = y;
    obj->lpos[2] = z;
}


/*
 * GXGetLightPos
 *
 * Reads back the position of a light.
 */
void GXGetLightPos(const GXLightObj* lt_obj, f32* x, f32* y, f32* z) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXGetLightPos");

    *x = obj->lpos[0];
    *y = obj->lpos[1];
    *z = obj->lpos[2];
}


/*
 * GXInitLightDir
 *
 * Sets the direction of a light (stored negated).
 */
// 0x8023BBF0
void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;

    obj->ldir[0] = -nx;
    obj->ldir[1] = -ny;
    obj->ldir[2] = -nz;
}


/*
 * GXGetLightDir
 *
 * Reads back the direction of a light.
 */
void GXGetLightDir(const GXLightObj* lt_obj, f32* nx, f32* ny, f32* nz) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;

    *nx = -obj->ldir[0];
    *ny = -obj->ldir[1];
    *nz = -obj->ldir[2];
}


/*
 * GXInitSpecularDir
 *
 * Sets the direction and position of a specular light from a unit
 * direction.
 */
void GXInitSpecularDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    f32 mag;
    f32 vx;
    f32 vy;
    f32 vz;
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitSpecularDir");

    vx = -nx;
    vy = -ny;
    vz = -nz + 1.0f;

    mag = (vx * vx) + (vy * vy) + (vz * vz);
    if (mag != 0.0f) {
        mag = 1.0f / sqrtf(mag);
    }
    
    obj->ldir[0] = vx * mag;
    obj->ldir[1] = vy * mag;
    obj->ldir[2] = vz * mag;
    obj->lpos[0] = nx * -1000000000000000000.0f;
    obj->lpos[1] = ny * -1000000000000000000.0f;
    obj->lpos[2] = nz * -1000000000000000000.0f;
}


/*
 * GXInitSpecularDirHA
 *
 * Sets a specular light from a direction and a half-angle vector.
 */
void GXInitSpecularDirHA(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz, f32 hx, f32 hy, f32 hz) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitSpecularHA");

    obj->ldir[0] = hx;
    obj->ldir[1] = hy;
    obj->ldir[2] = hz;
    obj->lpos[0] = nx * -1000000000000000000.0f;
    obj->lpos[1] = ny * -1000000000000000000.0f;
    obj->lpos[2] = nz * -1000000000000000000.0f;
}


/*
 * GXInitLightColor
 *
 * Sets the colour of a light.
 */
// 0x8023BC10
void GXInitLightColor(GXLightObj* lt_obj, GXColor color) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXInitLightColor");

    obj->Color = *(u32*)&color;
}


/*
 * GXGetLightColor
 *
 * Reads back the colour of a light.
 */
void GXGetLightColor(const GXLightObj* lt_obj, GXColor* color) {
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXGetLightColor");

    *(u32*)color = *(u32*)&obj->Color;
}


#if DEBUG
#define WRITE_SOME_LIGHT_REG1(val, addr) \
do {  \
    u32 xfData = (val); \
    GX_WRITE_U32(val); \
    VERIF_MTXLIGHT(addr, xfData); \
} while (0)

#define WRITE_SOME_LIGHT_REG2(val, addr) \
do {  \
    f32 xfData = (val); \
    GX_WRITE_F32(val); \
    VERIF_MTXLIGHT(addr, *(u32 *)&xfData); \
} while (0)
#else
#define WRITE_SOME_LIGHT_REG1(val, addr) GX_WRITE_U32(val)
#define WRITE_SOME_LIGHT_REG2(val, addr) GX_WRITE_F32(val)
#endif

/*
 * ConvLightID2Num
 *
 * Maps a light id bit to its slot number (debug builds only).
 */
static inline u32 ConvLightID2Num(GXLightID id) {
    switch (id) {
    case GX_LIGHT0: return 0;
    case GX_LIGHT1: return 1;
    case GX_LIGHT2: return 2;
    case GX_LIGHT3: return 3;
    case GX_LIGHT4: return 4;
    case GX_LIGHT5: return 5;
    case GX_LIGHT6: return 6;
    case GX_LIGHT7: return 7;
    default:        return 8;
    }
}

/*
 * PushLight
 *
 * Streams the 16 words of a light object into the XF light memory: three
 * zero words, the colour, then the attenuation, position and direction
 * vectors as paired singles.
 */
static inline void PushLight(const register GXLightObj* lt_obj, register volatile void* dest) {
    register u32 zero, color;
    register f32 a0_a1, a2_k0, k1_k2;
    register f32 px_py, pz_dx, dy_dz;
#ifdef __MWERKS__  // clang-format off
	asm volatile {
		lwz     color, 12(lt_obj)
		xor     zero, zero, zero
		psq_l   a0_a1, 16(lt_obj), 0, 0
		psq_l   a2_k0, 24(lt_obj), 0, 0
		psq_l   k1_k2, 32(lt_obj), 0, 0
		psq_l   px_py, 40(lt_obj), 0, 0
		psq_l   pz_dx, 48(lt_obj), 0, 0
		psq_l   dy_dz, 56(lt_obj), 0, 0

		stw     zero,  0(dest)
		stw     zero,  0(dest)
		stw     zero,  0(dest)
		stw     color, 0(dest)
		psq_st  a0_a1, 0(dest), 0, 0
		psq_st  a2_k0, 0(dest), 0, 0
		psq_st  k1_k2, 0(dest), 0, 0
		psq_st  px_py, 0(dest), 0, 0
		psq_st  pz_dx, 0(dest), 0, 0
		psq_st  dy_dz, 0(dest), 0, 0
	}
#endif  // clang-format on
}

/*
 * GXLoadLightObjImm
 *
 * Writes a light object to the XF light memory slot selected by the
 * light id, through the FIFO.
 */
// 0x8023BC20
void GXLoadLightObjImm(const GXLightObj* lt_obj, GXLightID light) {
    u32 addr;
    u32 idx;
    __GXLightObjInt_struct* obj;

    ASSERTMSG(lt_obj != NULL, "Light Object Pointer is null");
    obj = (__GXLightObjInt_struct*)lt_obj;
    CHECK_GXBEGIN("GXLoadLightObjImm");

#if DEBUG
    idx = ConvLightID2Num(light);
#else
    idx = 31 - __cntlzw(light);
#endif

    ASSERTMSG(idx < 8, "GXLoadLightObjImm: Invalid Light Id");
    idx &= 7;

    addr = idx * 0x10 + 0x600;
    GX_XF_LOAD_REGS(16 - 1, addr);

#if DEBUG
    WRITE_SOME_LIGHT_REG1(0, addr);
    WRITE_SOME_LIGHT_REG1(0, addr + 1);
    WRITE_SOME_LIGHT_REG1(0, addr + 2);
    WRITE_SOME_LIGHT_REG1(obj->Color, addr + 3);
    WRITE_SOME_LIGHT_REG2(obj->a[0], addr + 4);
    WRITE_SOME_LIGHT_REG2(obj->a[1], addr + 5);
    WRITE_SOME_LIGHT_REG2(obj->a[2], addr + 6);
    WRITE_SOME_LIGHT_REG2(obj->k[0], addr + 7);
    WRITE_SOME_LIGHT_REG2(obj->k[1], addr + 8);
    WRITE_SOME_LIGHT_REG2(obj->k[2], addr + 9);
    WRITE_SOME_LIGHT_REG2(obj->lpos[0], addr + 10);
    WRITE_SOME_LIGHT_REG2(obj->lpos[1], addr + 11);
    WRITE_SOME_LIGHT_REG2(obj->lpos[2], addr + 12);
    WRITE_SOME_LIGHT_REG2(obj->ldir[0], addr + 13);
    WRITE_SOME_LIGHT_REG2(obj->ldir[1], addr + 14);
    WRITE_SOME_LIGHT_REG2(obj->ldir[2], addr + 15);
#else
    PushLight(lt_obj, &GXWGFifo);
#endif

    __GXData->bpSentNot = 1;
}


/*
 * GXLoadLightObjIndx
 *
 * Loads a light object from an indexed array into an XF light slot.
 */
void GXLoadLightObjIndx(u32 lt_obj_indx, GXLightID light) {
    u32 reg;
    u32 addr;
    u32 idx;

    CHECK_GXBEGIN("GXLoadLightObjIndx");

#if DEBUG
    idx = ConvLightID2Num(light);
#else
    idx = 31 - __cntlzw(light);
#endif

    ASSERTMSG(idx < 8, "GXLoadLightObjIndx: Invalid Light Id");
    idx &= 7;

    addr = idx * 0x10 + 0x600;
    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 12, 0, addr);
    SET_REG_FIELD(__LINE__, reg, 4, 12, 0xF);
    SET_REG_FIELD(__LINE__, reg, 16, 16, lt_obj_indx);
    GX_WRITE_U8(0x38);
    GX_WRITE_U32(reg);
#if DEBUG
    __GXShadowIndexState(7, reg);
#endif
    __GXData->bpSentNot = 1;
}


#define GXCOLOR_AS_U32(color) (*((u32*)&(color)))

/*
 * GXSetChanAmbColor
 *
 * Sets the ambient colour of a colour or alpha channel.
 */
// 0x8023BCA0
void GXSetChanAmbColor(GXChannelID chan, GXColor amb_color) {
    u32 reg = 0;
    u32 rgb;
    u32 colIdx;

    CHECK_GXBEGIN("GXSetChanAmbColor");

    switch (chan) {
    case GX_COLOR0:
        reg = __GXData->ambColor[GX_COLOR0];
        rgb = GXCOLOR_AS_U32(amb_color) >> 8;
        SC_XF_AMBIENT0_F_SET_RGB(689, reg, rgb);
        colIdx = 0;
        break;
    case GX_COLOR1:
        reg = __GXData->ambColor[GX_COLOR1];
        rgb = GXCOLOR_AS_U32(amb_color) >> 8;
        SC_XF_AMBIENT1_F_SET_RGB(704, reg, rgb);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = __GXData->ambColor[GX_COLOR0];
        SC_XF_AMBIENT0_F_SET_ALPHA(710, reg, amb_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = __GXData->ambColor[GX_COLOR1];
        SC_XF_AMBIENT1_F_SET_ALPHA(716, reg, amb_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = GXCOLOR_AS_U32(amb_color);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = GXCOLOR_AS_U32(amb_color);
        colIdx = 1;
        break;
    default:
        ASSERTMSG(0, "GXSetChanAmbColor: Invalid Channel Id");
        return;
    }

    __GXData->dirtyState |= (0x100 << colIdx);
    __GXData->ambColor[colIdx] = reg;
}


/*
 * GXSetChanMatColor
 *
 * Sets the material colour of a colour or alpha channel.
 */
// 0x8023BD80
void GXSetChanMatColor(GXChannelID chan, GXColor mat_color) {
    u32 reg = 0;
    u32 rgb;
    u32 colIdx;

    CHECK_GXBEGIN("GXSetChanMatColor");

    switch (chan) {
    case GX_COLOR0:
        reg = __GXData->matColor[GX_COLOR0];
        rgb = GXCOLOR_AS_U32(mat_color) >> 8;
        SC_XF_MATERIAL0_F_SET_RGB(790, reg, rgb);
        colIdx = 0;
        break;
    case GX_COLOR1:
        reg = __GXData->matColor[GX_COLOR1];
        rgb = GXCOLOR_AS_U32(mat_color) >> 8;
        SC_XF_MATERIAL1_F_SET_RGB(805, reg, rgb);
        colIdx = 1;
        break;
    case GX_ALPHA0:
        reg = __GXData->matColor[GX_COLOR0];
        SC_XF_MATERIAL0_F_SET_ALPHA(811, reg, mat_color.a);
        colIdx = 0;
        break;
    case GX_ALPHA1:
        reg = __GXData->matColor[GX_COLOR1];
        SC_XF_MATERIAL1_F_SET_ALPHA(817, reg, mat_color.a);
        colIdx = 1;
        break;
    case GX_COLOR0A0:
        reg = GXCOLOR_AS_U32(mat_color);
        colIdx = 0;
        break;
    case GX_COLOR1A1:
        reg = GXCOLOR_AS_U32(mat_color);
        colIdx = 1;
        break;
    default:
        ASSERTMSG(0, "GXSetChanMatColor: Invalid Channel Id");
        return;
    }

    __GXData->dirtyState |= (0x400 << colIdx);
    __GXData->matColor[colIdx] = reg;
}


/*
 * GXSetNumChans
 *
 * Sets the number of colour channels.
 */
// 0x8023BE60
void GXSetNumChans(u8 nChans) {
    CHECK_GXBEGIN("GXSetNumChans");
    ASSERTMSG(nChans <= 2, "GXSetNumChans: nChans > 2");

    SC_GEN_MODE_SET_NCOL(874, __GXData->genMode, nChans);
    __GXData->dirtyState |= 0x1000004; // generation mode, colour channel count
}


/*
 * GXSetChanCtrl
 *
 * Programs the lighting control of a colour or alpha channel: light
 * enable, ambient and material sources, light mask, diffuse and
 * attenuation functions.
 */
// 0x8023BE90
void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src, u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn) {
    u32 reg;
    u32 idx;

    CHECK_GXBEGIN("GXSetChanCtrl");

    ASSERTMSG(chan >= GX_COLOR0 && chan <= GX_COLOR1A1, "GXSetChanCtrl: Invalid Channel Id");

#if DEBUG
    if (chan == GX_COLOR0A0) {
        idx = 0;
    } else if (chan == GX_COLOR1A1) {
        idx = 1;
    } else {
        idx = chan;
    }
#else
    idx = chan & 0x3;
#endif

    reg = 0;
    SC_XF_COLOR0CNTRL_F_SET_LIGHTFUNC(921, reg, enable);
    SC_XF_COLOR0CNTRL_F_SET_MATERIAL_SRC(922, reg, mat_src);
    SC_XF_COLOR0CNTRL_F_SET_AMBIENT_SRC(923, reg, amb_src);
    
    if (attn_fn == GX_AF_SPEC) {
        diff_fn = GX_DF_NONE;
    }
    SC_XF_COLOR0CNTRL_F_SET_DIFFUSEATTEN(925, reg, diff_fn);
    SC_XF_COLOR0CNTRL_F_SET_ATTENENABLE(926, reg, (attn_fn != GX_AF_NONE));
    SC_XF_COLOR0CNTRL_F_SET_ATTENSELECT(927, reg, (attn_fn != GX_AF_SPEC));

    SC_XF_COLOR0CNTRL_F_SET_LIGHT0123(939, reg, light_mask & 0xF);
    SC_XF_COLOR0CNTRL_F_SET_LIGHT4567(940, reg, (light_mask & 0xF0) >> 4);

    __GXData->chanCtrl[idx] = reg;
    __GXData->dirtyState |= (0x1000<<idx); // channel control dirty bit

    if (chan == GX_COLOR0A0) {
        __GXData->chanCtrl[GX_ALPHA0] = reg;
        __GXData->dirtyState |= (0x00001000|0x00004000); // controls 0 and 2
    } else if (chan == GX_COLOR1A1) {
        __GXData->chanCtrl[GX_ALPHA1] = reg;
        __GXData->dirtyState |= (0x00002000|0x00008000); // controls 1 and 3
    }
}

