// Reference: zeldaret/tp libs/revolution/src/gx/GXGeometry.c, doldecomp/ogws src/revolution/GX/GXGeometry.c
#include <stddef.h>
#include <revolution/gx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

/*
 * __GXSetTexGen
 *
 * Writes the XF texgen registers whose dirty bits are set in tgState: the
 * texture coordinate count (bit 25) and the per-coordinate texgen control
 * and dual-texgen words (bits 16-23).
 */
static inline void __GXSetTexGen(u32 tgState) {
    u32 d, i, tgIndx = 0x1040; // XF texgen control of coordinate 0
    u32 dtgIndx = 0x1050;      // XF dual-texgen control of coordinate 0

    if (tgState & 0x2000000) { // texture coordinate count
        d = __GXData->genMode & 0xF;
        GX_WRITE_XF_REG(0x103F, d, 0); // XF texture coordinate count
    }

    d = (tgState & 0xFF0000) >> 16; // per-coordinate dirty bits
    i = 0;
    while (d) {
        if (d & 0x1) {
            GX_WRITE_XF_REG(tgIndx, __GXData->texGenCtrl[i], 0);
            GX_WRITE_XF_REG(dtgIndx, __GXData->texGenCtrl2[i], 0);
        }
    
        dtgIndx++;
        tgIndx++;
        i++;
        d >>= 1;    
    }
}

/*
 * __GXSetLightChan
 *
 * Writes the XF colour channel registers whose dirty bits are set in chState:
 * the channel count (bit 24) and the channel controls (bits 12-15).
 */
static inline void __GXSetLightChan(u32 chState) {
    u32 d, i, chIndx = 0x100E; // XF colour 0 channel control

    if (chState & 0x1000000) { // colour channel count
        d = ((__GXData->genMode & 0x70) >> 4);
        GX_WRITE_XF_REG(0x1009, d, 0); // XF colour channel count
    }

    d = (chState & 0xF000) >> 12; // per-channel dirty bits
    i = 0;

    while (d) {
        if (d & 0x1) {
            GX_WRITE_XF_REG(chIndx, __GXData->chanCtrl[i], 0);
        }

        i++;
        chIndx++;
        d >>= 1;
    }
}

/*
 * __GXSetAmbMat
 *
 * Writes the XF ambient and material colour registers whose dirty bits
 * (8-11) are set in amState.
 */
static inline void __GXSetAmbMat(u32 amState) {
    if (amState & 0x100) {
        GX_WRITE_XF_REG(0x100A, __GXData->ambColor[0], 0); // XF ambient 0
    }

    if (amState & 0x200) {
        GX_WRITE_XF_REG(0x100B, __GXData->ambColor[1], 0); // XF ambient 1
    }

    if (amState & 0x400) {
        GX_WRITE_XF_REG(0x100C, __GXData->matColor[0], 0); // XF material 0
    }

    if (amState & 0x800) {
        GX_WRITE_XF_REG(0x100D, __GXData->matColor[1], 0); // XF material 1
    }
}

/*
 * __GXSetDirtyState
 *
 * Flushes the pending state selected by dirtyState: bits 0-4 are the BP and
 * vertex setup (SU texture sizes, BP mask, generation mode, vertex
 * descriptor, vertex attribute format); bits 8-11 ambient and material
 * colours, 12-15 and 24 colour channels, 16-23 and 25 texgen, 26 matrix
 * indices, 27 projection, 28 viewport are sent to the XF.
 */
// 0x8023AAE0
void __GXSetDirtyState(void) {
    u32 dState = __GXData->dirtyState;

    if (dState & 1) {
        __GXSetSUTexRegs();
    }
    if (dState & 2) {
        __GXUpdateBPMask();
    }
    if (dState & 4) {
        __GXSetGenMode();
    }
    if (dState & 8) {
        __GXSetVCD();
    }
    if (dState & 0x10) {
        __GXSetVAT();
    }
    if (dState & 0x18) {
        __GXCalculateVLim();
    }

    dState &= 0xFFFFFF00; // drop the BP/vertex bits handled above

    if (dState) {
        u32 dStateLocal;

        dStateLocal = dState & 0x00000F00;

        if (dStateLocal) {
            __GXSetAmbMat(dStateLocal);
        }

        dStateLocal = dState & (0x01000000|0x0000F000); // channel count, channel controls

        if (dStateLocal) {
            __GXSetLightChan(dStateLocal);
        }

        dStateLocal = dState & (0x02000000|0x00FF0000); // coordinate count, texgens

        if (dStateLocal) {
            __GXSetTexGen(dStateLocal);
        }

        dStateLocal = dState & 0x04000000; // matrix index registers
        if (dStateLocal) {
            __GXSetMatrixIndex(GX_VA_PNMTXIDX);
            __GXSetMatrixIndex(GX_VA_TEX4MTXIDX);
        }

        dStateLocal = dState & 0x10000000; // viewport
        if (dStateLocal) {
            __GXSetViewport();
        }

        dStateLocal = dState & 0x8000000; // projection
        if (dStateLocal) {
            __GXSetProjection();
        }

        __GXData->bpSentNot = GX_TRUE;
    }

    __GXData->dirtyState = 0;
}

/*
 * GXBegin
 *
 * Starts a primitive: flushes dirty state, then writes the command byte
 * (vertex format | primitive type) and the vertex count to the FIFO.
 */
void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {
    ASSERTMSG(vtxfmt < GX_MAX_VTXFMT,   "GXBegin: Format Index is out of range");
    ASSERTMSG(!__GXinBegin, "GXBegin: called inside another GXBegin/GXEnd");

    if (__GXData->dirtyState != 0) {
        __GXSetDirtyState();
    }

#if DEBUG
    if (!__GXData->inDispList) {
        __GXVerifyState(vtxfmt);
    }
    __GXinBegin = 1;
#endif

    if (*(u32*)&__GXData->vNumNot == 0) {  // checks both vNum and bpSentNot
        __GXSendFlushPrim();
    }
    GX_WRITE_U8(vtxfmt | type);
    GX_WRITE_U16(nverts);
}

/*
 * __GXSendFlushPrim
 *
 * Pads the FIFO with a flush primitive (0x98) so the pipeline drains a
 * pending vertex group.
 */
// 0x8023AEB0
void __GXSendFlushPrim(void) {
    u32 i;
    u32 numD = __GXData->vNum * __GXData->vLim;

    GX_WRITE_U8(0x98); // flush primitive command
    GX_WRITE_U16(__GXData->vNum);
    for (i = 0; i < numD; i += 4) {
        GX_WRITE_U32(0);
    }
    __GXData->bpSentNot = 1;
}

/*
 * GXSetLineWidth
 *
 * Sets the width of lines and their texture offset mode.
 */
// 0x8023AF90
void GXSetLineWidth(u8 width, GXTexOffset texOffsets) {
    CHECK_GXBEGIN("GXSetLineWidth");
    SC_SU_LPSIZE_SET_LSIZE(448, __GXData->lpSize, width);
    SC_SU_LPSIZE_SET_LTOFF(449, __GXData->lpSize, texOffsets);
    GX_WRITE_RAS_REG(__GXData->lpSize);
    __GXData->bpSentNot = 0;
}

/*
 * GXGetLineWidth
 *
 * Reads back the line width and texture offset mode.
 */
void GXGetLineWidth(u8* width, GXTexOffset* texOffsets) {
    ASSERTMSG(width != NULL && texOffsets != NULL, "GXGet*: invalid null pointer");

    *width      = SU_LPSIZE_GET_LSIZE(__GXData->lpSize);
    *texOffsets = SU_LPSIZE_GET_LTOFF(__GXData->lpSize);
}

/*
 * GXSetPointSize
 *
 * Sets the size of points and their texture offset mode.
 */
// 0x8023AFD0
void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets) {
    CHECK_GXBEGIN("GXSetPointSize");
    SC_SU_LPSIZE_SET_PSIZE(492, __GXData->lpSize, pointSize);
    SC_SU_LPSIZE_SET_PTOFF(493, __GXData->lpSize, texOffsets);
    GX_WRITE_RAS_REG(__GXData->lpSize);
    __GXData->bpSentNot = 0;
}

/*
 * GXGetPointSize
 *
 * Reads back the point size and texture offset mode.
 */
void GXGetPointSize(u8* pointSize, GXTexOffset* texOffsets) {
    ASSERTMSG(pointSize != NULL && texOffsets != NULL, "GXGet*: invalid null pointer");

    *pointSize  = SU_LPSIZE_GET_PSIZE(__GXData->lpSize);
    *texOffsets = SU_LPSIZE_GET_PTOFF(__GXData->lpSize);
}

/*
 * GXEnableTexOffsets
 *
 * Enables line and point texture offsets for one texture coordinate.
 */
// 0x8023B010
void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable) {
    CHECK_GXBEGIN("GXEnableTexOffsets");

    ASSERTMSG(coord < GX_MAX_TEXCOORD, "GXEnableTexOffsets: Invalid coordinate Id");

    SC_SU_TS0_SET_LF(540, __GXData->suTs0[coord], line_enable);
    SC_SU_TS0_SET_PF(541, __GXData->suTs0[coord], point_enable);
    GX_WRITE_RAS_REG(__GXData->suTs0[coord]);
    __GXData->bpSentNot = 0;
}

/*
 * GXSetCullMode
 *
 * Sets which faces are culled. The API values and the hardware
 * encoding of the reject field differ by a swap of the two bits.
 */
// 0x8023B050
void GXSetCullMode(GXCullMode mode) {
    GXCullMode hwMode;

    CHECK_GXBEGIN("GXSetCullMode");

    // Swap bits to get the hardware representation.
    hwMode = (mode << 1 & 2) | (mode >> 1 & 1);

    SC_GEN_MODE_SET_REJECT_EN(577, __GXData->genMode, hwMode);
    __GXData->dirtyState |= 4;
}

/*
 * GXGetCullMode
 *
 * Reads back the culling mode.
 */
void GXGetCullMode(GXCullMode* mode) {
    GXCullMode hwMode = GEN_MODE_GET_REJECT_EN(__GXData->genMode);

    // Swap bits back to the API representation.
    *mode = (hwMode << 1 & 2) | (hwMode >> 1 & 1);
}

/*
 * GXSetCoPlanar
 *
 * Enables coplanar (Z-freeze) rendering and sends the register that
 * gates it.
 */
// 0x8023B080
void GXSetCoPlanar(GXBool enable) {
    u32 reg;

    CHECK_GXBEGIN("GXSetCoPlanar");

    SC_GEN_MODE_SET_ZFREEZE(622, __GXData->genMode, enable);
    reg = 0xFE080000; // BP mask register with only the Z-freeze bit enabled
    GX_WRITE_RAS_REG(reg);
    GX_WRITE_RAS_REG(__GXData->genMode);
    __GXData->bpSentNot = 0;
}

/*
 * __GXSetGenMode
 *
 * Sends the generation mode register to the BP.
 */
// 0x8023B0C0
void __GXSetGenMode(void) {
    GX_WRITE_RAS_REG(__GXData->genMode);
    __GXData->bpSentNot = 0;
}

