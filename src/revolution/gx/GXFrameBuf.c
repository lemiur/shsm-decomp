#include <revolution/gx.h>
#include <revolution/os.h>
#include <revolution/vi.h>

#include <revolution/gx/__gx.h>

GXRenderModeObj GXNtsc240Ds = {
    VI_TVMODE_NTSC_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc240DsAa = {
    VI_TVMODE_NTSC_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc240Int = {
    VI_TVMODE_NTSC_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc240IntAa = {
    VI_TVMODE_NTSC_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXNtsc480Int = {
    VI_TVMODE_NTSC_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc480IntAa = {
    VI_TVMODE_NTSC_INT,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXNtsc480Prog = {
    VI_TVMODE_NTSC_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXNtsc480ProgSoft = {
    VI_TVMODE_NTSC_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXNtsc480ProgAa = {
    VI_TVMODE_NTSC_PROG,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXMpal240Ds = {
    VI_TVMODE_MPAL_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal240DsAa = {
    VI_TVMODE_MPAL_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal240Int = {
    VI_TVMODE_MPAL_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal240IntAa = {
    VI_TVMODE_MPAL_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_MPAL_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXMpal480Int = {
    VI_TVMODE_MPAL_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal480IntAa = {
    VI_TVMODE_MPAL_INT,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXMpal480Prog = {
    VI_TVMODE_MPAL_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXMpal480ProgSoft = {
    VI_TVMODE_MPAL_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXMpal480ProgAa = {
    VI_TVMODE_MPAL_PROG,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXPal264Ds = {
    VI_TVMODE_PAL_DS,
    640, 264, 264,
    40, 11,
    640, 528,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal264DsAa = {
    VI_TVMODE_PAL_DS,
    640, 264, 264,
    40, 11,
    640, 528,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal264Int = {
    VI_TVMODE_PAL_INT,
    640, 264, 264,
    40, 23,
    640, 528,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal264IntAa = {
    VI_TVMODE_PAL_INT,
    640, 264, 264,
    40, 23,
    640, 528,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT,
    640, 528, 528,
    40, 23,
    640, 528,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXPal528Int = {
    VI_TVMODE_PAL_INT,
    640, 528, 528,
    40, 23,
    640, 528,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXPal524IntAa = {
    VI_TVMODE_PAL_INT,
    640, 264, 524,
    40, 23,
    640, 524,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXEurgb60Hz240Ds = {
    VI_TVMODE_EURGB60_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz240DsAa = {
    VI_TVMODE_EURGB60_DS,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz240Int = {
    VI_TVMODE_EURGB60_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz240IntAa = {
    VI_TVMODE_EURGB60_INT,
    640, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_TRUE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz480IntDf = {
    VI_TVMODE_EURGB60_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXEurgb60Hz480Int = {
    VI_TVMODE_EURGB60_INT,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz480IntAa = {
    VI_TVMODE_EURGB60_INT,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_DF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXEurgb60Hz480Prog = {
    VI_TVMODE_EURGB60_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

GXRenderModeObj GXEurgb60Hz480ProgSoft = {
    VI_TVMODE_EURGB60_PROG,
    640, 480, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 8, 8, 10, 12, 10, 8, 8 }
};

GXRenderModeObj GXEurgb60Hz480ProgAa = {
    VI_TVMODE_EURGB60_PROG,
    640, 242, 480,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    { 3, 2, 9, 6, 3, 10, 3, 2, 9, 6, 3, 10, 9, 2, 3, 6, 9, 10, 9, 2, 3, 6, 9, 10 },
    { 4, 8, 12, 16, 12, 8, 4 }
};

GXRenderModeObj GXRmHW = {
    VI_TVMODE_NTSC_DS,
    320, 240, 240,
    40, 0,
    640, 480,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 0, 21, 22, 21, 0, 0 }
};

// Shrinks a render mode by hor/ver pixels of overscan on each side.
void GXAdjustForOverscan(const GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver) {
    u16 hor2 = hor * 2;
    u16 ver2 = ver * 2;
    u32 verf;
    u32 mode;

    if (rmin != rmout) {
        *rmout = *rmin;
    }

    mode = rmin->viTVmode & 3;
    rmout->fbWidth = rmin->fbWidth - hor2;
    verf = (ver2 * rmin->efbHeight) / (u32)rmin->xfbHeight;
    rmout->efbHeight = rmin->efbHeight - verf;
    if (rmin->xFBmode == VI_XFBMODE_SF && mode == 0) {
        rmout->xfbHeight = rmin->xfbHeight - ver2 / 2;
    } else {
        rmout->xfbHeight = rmin->xfbHeight - ver2;
    }

    rmout->viWidth = rmin->viWidth - hor2;

    if (mode == 1) {
        rmout->viHeight = rmin->viHeight - (ver2 * 2);
    } else {
        rmout->viHeight = rmin->viHeight - ver2;
    }
 
    rmout->viXOrigin = rmin->viXOrigin + hor;
    rmout->viYOrigin = rmin->viYOrigin + ver;
}

// Sets the EFB rectangle that GXCopyDisp copies to the XFB.
// 0x8023B0F0
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    CHECK_GXBEGIN("GXSetDispCopySrc");

    __GXData->cpDispSrc = 0;
    
    SET_REG_FIELD(__LINE__, __GXData->cpDispSrc, 10,  0, left);
    SET_REG_FIELD(__LINE__, __GXData->cpDispSrc, 10, 10, top);
    SET_REG_FIELD(__LINE__, __GXData->cpDispSrc,  8, 24, 0x49);

    __GXData->cpDispSize = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpDispSize, 10,  0, wd - 1);
    SET_REG_FIELD(__LINE__, __GXData->cpDispSize, 10, 10, ht - 1);
    SET_REG_FIELD(__LINE__, __GXData->cpDispSize,  8, 24, 0x4A);
}


// Sets the EFB rectangle that GXCopyTex copies to a texture.
// 0x8023B130
void GXSetTexCopySrc(u16 left, u16 top, u16 wd, u16 ht) {
    CHECK_GXBEGIN("GXSetTexCopySrc");

    __GXData->cpTexSrc = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpTexSrc, 10,  0, left);
    SET_REG_FIELD(__LINE__, __GXData->cpTexSrc, 10, 10, top);
    SET_REG_FIELD(__LINE__, __GXData->cpTexSrc,  8, 24, 0x49);

    __GXData->cpTexSize = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpTexSize, 10,  0, wd - 1);
    SET_REG_FIELD(__LINE__, __GXData->cpTexSize, 10, 10, ht - 1);
    SET_REG_FIELD(__LINE__, __GXData->cpTexSize,  8, 24, 0x4A);
}

// Sets the XFB row stride (from the width in pixels) for display copies.
// 0x8023B170
void GXSetDispCopyDst(u16 wd, u16 ht) {
    u16 stride;

    ASSERTMSG((wd & 15) == 0, "GXSetDispCopyDst: Width must be a multiple of 16");
    CHECK_GXBEGIN("GXSetDispCopyDst");

    stride = wd * 2;
    __GXData->cpDispStride = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpDispStride, 10,  0, stride >> 5);
    SET_REG_FIELD(__LINE__, __GXData->cpDispStride,  8, 24, 0x4D);
}

// Sets the texture format, stride and mipmap flag for texture copies.
// 0x8023B1A0
void GXSetTexCopyDst(u16 wd, u16 ht, GXTexFmt fmt, GXBool mipmap) {
    u32 rowTiles;
    u32 colTiles;
    u32 cmpTiles;
    u32 peTexFmt;
    u32 peTexFmtH;

    CHECK_GXBEGIN("GXSetTexCopyDst");

    __GXData->cpTexZ = 0;
    peTexFmt = fmt & 0xF;
    ASSERTMSG1(peTexFmt < 13, "%s: invalid texture format", "GXSetTexCopyDst");

    if (fmt == GX_TF_Z16) {
        peTexFmt = 0xB;
    }

    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_IA8:
    case GX_CTF_YUVA8:
        SET_REG_FIELD(__LINE__, __GXData->cpTex, 2, 15, 3);
        break;
    default:
        SET_REG_FIELD(__LINE__, __GXData->cpTex, 2, 15, 2);
        break;
    }

    __GXData->cpTexZ = (fmt & _GX_TF_ZTF) == _GX_TF_ZTF;
    peTexFmtH = (peTexFmt & 8) >> 3;
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 3, peTexFmtH);
    peTexFmt = peTexFmt & 7;
    __GetImageTileCount(fmt, wd, ht, &rowTiles, &colTiles, &cmpTiles);

    __GXData->cpTexStride = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpTexStride, 10,  0, rowTiles * cmpTiles);
    SET_REG_FIELD(__LINE__, __GXData->cpTexStride,  8, 24, 0x4D);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 9, mipmap);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 3, 4, peTexFmt);
}

// Selects the frame-to-field mode of display copies.
// 0x8023B2B0
void GXSetDispCopyFrame2Field(GXCopyMode mode) {
    CHECK_GXBEGIN("GXSetDispCopyFrame2Field");
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 2, 12, mode);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 2, 12, 0);
}

// Sets the top and bottom clamping of EFB copies.
// 0x8023B2D0
void GXSetCopyClamp(GXFBClamp clamp) {
    u8 clmpB;
    u8 clmpT;

    CHECK_GXBEGIN("GXSetCopyClamp");

    clmpT = (clamp & GX_CLAMP_TOP) == 1;
    clmpB = (clamp & GX_CLAMP_BOTTOM) == 2;

    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 1, 0, clmpT);
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 1, 1, clmpB);

    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 0, clmpT);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 1, clmpB);
}

// Computes how many XFB lines a copy of efbHt lines produces for a vertical scale.
static u32 __GXGetNumXfbLines(u32 efbHt, u32 iScale) {
    u32 count;
    u32 realHt;
    u32 iScaleD;

    count = (efbHt - 1) * 256;
    realHt = (count / iScale) + 1;

    iScaleD = iScale;

    if (iScaleD > 128 && iScaleD < 256) {
        while (iScaleD % 2 == 0) {
            iScaleD /= 2;
        }

        if (efbHt % iScaleD == 0) {
            realHt++;
        }
    }

    if (realHt > 1024) {
        realHt = 1024;
    }

    return realHt;
}

// Returns the number of XFB lines produced from efbHeight lines at yScale.
u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
    u32 iScale;
    ASSERTMSG(yScale >= 1.0f, "GXGetNumXfbLines: Vertical scale must be >= 1.0");

    iScale = (u32)(256.0f / yScale) & 0x1FF;
    return __GXGetNumXfbLines(efbHeight, iScale);
}

// Finds the vertical scale that turns efbHeight lines into exactly xfbHeight lines.
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    f32 fScale;
    f32 yScale;
    u32 iScale;
    u32 tgtHt;
    u32 realHt;

    ASSERTMSG(xfbHeight <= 1024, "GXGetYScaleFactor: Display copy only supports up to 1024 lines.\n");
    ASSERTMSG(efbHeight <= xfbHeight, "GXGetYScaleFactor: EFB height should not be greater than XFB height.\n");

    tgtHt = xfbHeight;
    yScale = (f32)xfbHeight / (f32)efbHeight;
    iScale = (u32)(256.0f / yScale) & 0x1FF;
    realHt = __GXGetNumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = (f32)tgtHt / (f32)efbHeight;
        iScale = (u32)(256.0f / yScale) & 0x1FF;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = (f32)tgtHt / (f32)efbHeight;
        iScale = (u32)(256.0f / yScale) & 0x1FF;
        realHt = __GXGetNumXfbLines(efbHeight, iScale);
    }

    return fScale;
}

// Sets the vertical scale of display copies; returns the XFB line count.
// 0x8023B300
u32 GXSetDispCopyYScale(f32 vscale) {
    u8 enable;
    u32 iScale;
    u32 ht;
    u32 reg;

    CHECK_GXBEGIN("GXSetDispCopyYScale");

    ASSERTMSG(vscale >= 1.0f, "GXSetDispCopyYScale: Vertical scale must be >= 1.0");

    iScale = (u32) (256.0f / vscale) & 0x1FF;
    enable = (iScale != 256);

    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 9, 0, iScale);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x4E);
    GX_WRITE_RAS_REG(reg);
    __GXData->bpSentNot = 0;
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 1, 10, enable);
    ht = GET_REG_FIELD(__GXData->cpDispSize, 10, 10) + 1;
    return __GXGetNumXfbLines(ht, iScale);
}

// Sets the colour and Z value that EFB copies clear to.
// 0x8023B3D0
void GXSetCopyClear(GXColor clear_clr, u32 clear_z) {
    u32 reg;

    CHECK_GXBEGIN("GXSetCopyClear");
    ASSERTMSG(clear_z <= 0xFFFFFF, "GXSetCopyClear: Z clear value is out of range"); // 24-bit Z

    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 8, 0, clear_clr.r);
    SET_REG_FIELD(__LINE__, reg, 8, 8, clear_clr.a);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x4F);
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 8, 0, clear_clr.b);
    SET_REG_FIELD(__LINE__, reg, 8, 8, clear_clr.g);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x50);
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 24, 0, clear_z);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x51);
    GX_WRITE_RAS_REG(reg);
    __GXData->bpSentNot = 0;
}

// Sets the multisample pattern and the vertical filter of EFB copies.
// 0x8023B450
void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf, const u8 vfilter[7]) {
    u32 msLoc[4];
    u32 coeff0;
    u32 coeff1;

    CHECK_GXBEGIN("GXSetCopyFilter");

    if (aa != 0) {
        msLoc[0] = 0;
        SET_REG_FIELD(__LINE__, msLoc[0], 4,  0, sample_pattern[0][0]);
        SET_REG_FIELD(__LINE__, msLoc[0], 4,  4, sample_pattern[0][1]);
        SET_REG_FIELD(__LINE__, msLoc[0], 4,  8, sample_pattern[1][0]);
        SET_REG_FIELD(__LINE__, msLoc[0], 4, 12, sample_pattern[1][1]);
        SET_REG_FIELD(__LINE__, msLoc[0], 4, 16, sample_pattern[2][0]);
        SET_REG_FIELD(__LINE__, msLoc[0], 4, 20, sample_pattern[2][1]);
        SET_REG_FIELD(__LINE__, msLoc[0], 8, 24, 1);

        msLoc[1] = 0;
        SET_REG_FIELD(__LINE__, msLoc[1], 4,  0, sample_pattern[3][0]);
        SET_REG_FIELD(__LINE__, msLoc[1], 4,  4, sample_pattern[3][1]);
        SET_REG_FIELD(__LINE__, msLoc[1], 4,  8, sample_pattern[4][0]);
        SET_REG_FIELD(__LINE__, msLoc[1], 4, 12, sample_pattern[4][1]);
        SET_REG_FIELD(__LINE__, msLoc[1], 4, 16, sample_pattern[5][0]);
        SET_REG_FIELD(__LINE__, msLoc[1], 4, 20, sample_pattern[5][1]);
        SET_REG_FIELD(__LINE__, msLoc[1], 8, 24, 2);

        msLoc[2] = 0;
        SET_REG_FIELD(__LINE__, msLoc[2], 4,  0, sample_pattern[6][0]);
        SET_REG_FIELD(__LINE__, msLoc[2], 4,  4, sample_pattern[6][1]);
        SET_REG_FIELD(__LINE__, msLoc[2], 4,  8, sample_pattern[7][0]);
        SET_REG_FIELD(__LINE__, msLoc[2], 4, 12, sample_pattern[7][1]);
        SET_REG_FIELD(__LINE__, msLoc[2], 4, 16, sample_pattern[8][0]);
        SET_REG_FIELD(__LINE__, msLoc[2], 4, 20, sample_pattern[8][1]);
        SET_REG_FIELD(__LINE__, msLoc[2], 8, 24, 3);

        msLoc[3] = 0;
        SET_REG_FIELD(__LINE__, msLoc[3], 4,  0, sample_pattern[9][0]);
        SET_REG_FIELD(__LINE__, msLoc[3], 4,  4, sample_pattern[9][1]);
        SET_REG_FIELD(__LINE__, msLoc[3], 4,  8, sample_pattern[10][0]);
        SET_REG_FIELD(__LINE__, msLoc[3], 4, 12, sample_pattern[10][1]);
        SET_REG_FIELD(__LINE__, msLoc[3], 4, 16, sample_pattern[11][0]);
        SET_REG_FIELD(__LINE__, msLoc[3], 4, 20, sample_pattern[11][1]);
        SET_REG_FIELD(__LINE__, msLoc[3], 8, 24, 4);
    } else {
        /* Centre sample for every pixel: register ids 1-4, all six offsets 6 (x, y). */
        msLoc[0] = 0x01666666;
        msLoc[1] = 0x02666666; // register id 2
        msLoc[2] = 0x03666666; // register id 3
        msLoc[3] = 0x04666666; // register id 4
    }

    GX_WRITE_RAS_REG(msLoc[0]);
    GX_WRITE_RAS_REG(msLoc[1]);
    GX_WRITE_RAS_REG(msLoc[2]);
    GX_WRITE_RAS_REG(msLoc[3]);

    coeff0 = 0;
    SET_REG_FIELD(__LINE__, coeff0, 8, 24, 0x53);
    coeff1 = 0;
    SET_REG_FIELD(__LINE__, coeff1, 8, 24, 0x54);

    if (vf != 0) {
        SET_REG_FIELD(__LINE__, coeff0, 6,  0, vfilter[0]);
        SET_REG_FIELD(__LINE__, coeff0, 6,  6, vfilter[1]);
        SET_REG_FIELD(__LINE__, coeff0, 6, 12, vfilter[2]);
        SET_REG_FIELD(__LINE__, coeff0, 6, 18, vfilter[3]);
        SET_REG_FIELD(__LINE__, coeff1, 6,  0, vfilter[4]);
        SET_REG_FIELD(__LINE__, coeff1, 6,  6, vfilter[5]);
        SET_REG_FIELD(__LINE__, coeff1, 6, 12, vfilter[6]);
    } else {
        SET_REG_FIELD(__LINE__, coeff0, 6,  0, 0);
        SET_REG_FIELD(__LINE__, coeff0, 6,  6, 0);
        SET_REG_FIELD(__LINE__, coeff0, 6, 12, 21);
        SET_REG_FIELD(__LINE__, coeff0, 6, 18, 22);
        SET_REG_FIELD(__LINE__, coeff1, 6,  0, 21);
        SET_REG_FIELD(__LINE__, coeff1, 6,  6,  0);
        SET_REG_FIELD(__LINE__, coeff1, 6, 12,  0);
    }

    GX_WRITE_RAS_REG(coeff0);
    GX_WRITE_RAS_REG(coeff1);
    __GXData->bpSentNot = 0;
}

// Sets the gamma applied by display copies.
// 0x8023B640
void GXSetDispCopyGamma(GXGamma gamma) {
    CHECK_GXBEGIN("GXSetDispCopyGamma");
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 2, 7, gamma);
}

#if DEBUG
// Checks the copy parameters before a display or texture copy (debug builds only).
static void __GXVerifCopy(void* dest, u8 clear) {
    u8 clmpT;
    u8 clmpB;
    u32 x0;
    u32 y0;
    u32 dx;
    u32 dy;

    CHECK_GXBEGIN("GXCopyDisp");

    clmpT = GET_REG_FIELD(__GXData->cpDisp, 1, 0);
    clmpB = GET_REG_FIELD(__GXData->cpDisp, 1, 1);
    x0 = GET_REG_FIELD(__GXData->cpDispSrc, 10, 0);
    dx = GET_REG_FIELD(__GXData->cpDispSize, 10, 0) + 1;
    y0 = GET_REG_FIELD(__GXData->cpDispSrc, 10, 10);
    dy = GET_REG_FIELD(__GXData->cpDispSize, 10, 10) + 1;

    ASSERTMSG(clmpT || y0 != 0, "GXCopy: Have to set GX_CLAMP_TOP if source top == 0");
    ASSERTMSG(clmpB || y0 + dy <= 528, "GXCopy: Have to set GX_CLAMP_BOTTOM if source bottom > 528");
    ASSERTMSG((__GXData->peCtrl & 7) != 3 || clear == 0, "GXCopy: Can not do clear while pixel type is Z");
    
    if ((__GXData->peCtrl & 7) == 5) {
        ASSERTMSG(clear == 0, "GXCopy: Can not clear YUV framebuffer");
        ASSERTMSG((x0 & 3) == 0, "GXCopy: Source x is not multiple of 4 for YUV copy");
        ASSERTMSG((y0 & 3) == 0, "GXCopy: Source y is not multiple of 4 for YUV copy");
        ASSERTMSG((dx & 3) == 0, "GXCopy: Source width is not multiple of 4 for YUV copy");
        ASSERTMSG((dy & 3) == 0, "GXCopy: Source height is not multiple of 4 for YUV copy");
    } else {
        ASSERTMSG((x0 & 1) == 0, "GXCopy: Source x is not multiple of 2 for RGB copy");
        ASSERTMSG((y0 & 1) == 0, "GXCopy: Source y is not multiple of 2 for RGB copy");
        ASSERTMSG((dx & 1) == 0, "GXCopy: Source width is not multiple of 2 for RGB copy");
        ASSERTMSG((dy & 1) == 0, "GXCopy: Source height is not multiple of 2 for RGB copy");
    }

    ASSERTMSG(((u32)dest & 0x1F) == 0, "GXCopy: Display destination address not 32B aligned");
}
#endif

// Copies the EFB to the XFB at dest, optionally clearing the EFB.
// 0x8023B660
void GXCopyDisp(void* dest, GXBool clear) {
    u32 reg;
    u32 tempPeCtrl;
    u32 phyAddr;
    u8 changePeCtrl;

    CHECK_GXBEGIN("GXCopyDisp");

#if DEBUG
    __GXVerifCopy(dest, clear);
#endif

    if (clear) {
        reg = __GXData->zmode;
        SET_REG_FIELD(__LINE__, reg, 1, 0, 1);
        SET_REG_FIELD(__LINE__, reg, 3, 1, 7);
        GX_WRITE_RAS_REG(reg);

        reg = __GXData->cmode0;
        SET_REG_FIELD(__LINE__, reg, 1, 0, 0);
        SET_REG_FIELD(__LINE__, reg, 1, 1, 0);
        GX_WRITE_RAS_REG(reg);
    }

    changePeCtrl = FALSE;

    if ((clear || GET_REG_FIELD(__GXData->peCtrl, 3, 0) == 3) && GET_REG_FIELD(__GXData->peCtrl, 1, 6) == 1) {
        changePeCtrl = TRUE;
        tempPeCtrl = __GXData->peCtrl;
        SET_REG_FIELD(__LINE__, tempPeCtrl, 1, 6, 0);
        GX_WRITE_RAS_REG(tempPeCtrl);
    }

    GX_WRITE_RAS_REG(__GXData->cpDispSrc);
    GX_WRITE_RAS_REG(__GXData->cpDispSize);
    GX_WRITE_RAS_REG(__GXData->cpDispStride);

    phyAddr = GX_PHY_ADDR(dest);
    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 24, 0, phyAddr >> 5);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x4B);
    GX_WRITE_RAS_REG(reg);

    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 1, 11, clear);
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 1, 14, 1);
    SET_REG_FIELD(__LINE__, __GXData->cpDisp, 8, 24, 0x52);
    GX_WRITE_RAS_REG(__GXData->cpDisp);

    if (clear) {
        GX_WRITE_RAS_REG(__GXData->zmode);
        GX_WRITE_RAS_REG(__GXData->cmode0);
    }

    if (changePeCtrl) {
        GX_WRITE_RAS_REG(__GXData->peCtrl);
    }

    __GXData->bpSentNot = 0;
}

// Copies the EFB to a texture at dest, optionally clearing the EFB.
// 0x8023B7A0
void GXCopyTex(void* dest, GXBool clear) {
    u32 reg;
    u32 tempPeCtrl;
    u32 phyAddr;
    u8 changePeCtrl;

    CHECK_GXBEGIN("GXCopyTex");

#if DEBUG
    __GXVerifCopy(dest, clear);
#endif
    if (clear) {
        reg = __GXData->zmode;
        SET_REG_FIELD(__LINE__, reg, 1, 0, 1);
        SET_REG_FIELD(__LINE__, reg, 3, 1, 7);
        GX_WRITE_RAS_REG(reg);

        reg = __GXData->cmode0;
        SET_REG_FIELD(__LINE__, reg, 1, 0, 0);
        SET_REG_FIELD(__LINE__, reg, 1, 1, 0);
        GX_WRITE_RAS_REG(reg);
    }

    changePeCtrl = 0;
    tempPeCtrl = __GXData->peCtrl;

    if (__GXData->cpTexZ && GET_REG_FIELD(tempPeCtrl, 3, 0) != 3) {
        changePeCtrl = 1;
        SET_REG_FIELD(__LINE__, tempPeCtrl, 3, 0, 3);
    }

    if ((clear || GET_REG_FIELD(tempPeCtrl, 3, 0) == 3) && GET_REG_FIELD(tempPeCtrl, 1, 6) == 1) {
        changePeCtrl = 1;
        SET_REG_FIELD(__LINE__, tempPeCtrl, 1, 6, 0);
    }

    if (changePeCtrl) {
        GX_WRITE_RAS_REG(tempPeCtrl);
    }

    GX_WRITE_RAS_REG(__GXData->cpTexSrc);
    GX_WRITE_RAS_REG(__GXData->cpTexSize);
    GX_WRITE_RAS_REG(__GXData->cpTexStride);

    phyAddr = GX_PHY_ADDR(dest);
    reg = 0;
    SET_REG_FIELD(__LINE__, reg, 24, 0, phyAddr >> 5);
    SET_REG_FIELD(__LINE__, reg, 8, 24, 0x4B);
    GX_WRITE_RAS_REG(reg);

    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 11, clear);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 1, 14, 0);
    SET_REG_FIELD(__LINE__, __GXData->cpTex, 8, 24, 0x52);
    GX_WRITE_RAS_REG(__GXData->cpTex);

    if (clear) {
        GX_WRITE_RAS_REG(__GXData->zmode);
        GX_WRITE_RAS_REG(__GXData->cmode0);
    }

    if (changePeCtrl) {
        GX_WRITE_RAS_REG(__GXData->peCtrl);
    }

    __GXData->bpSentNot = 0;
}

// Resets the bounding box registers.
// 0x8023B900
void GXClearBoundingBox(void) {
    u32 reg;

    CHECK_GXBEGIN("GXClearBoundingBox");
    /* Reset the bounding-box registers 0x55 and 0x56 to 0x3FF. */
    reg = 0x550003FF;
    GX_WRITE_RAS_REG(reg);
    reg = 0x560003FF; // register 0x56
    GX_WRITE_RAS_REG(reg);
    __GXData->bpSentNot = 0;
}

// Reads the bounding box registers.
void GXReadBoundingBox(u16* left, u16* top, u16* right, u16* bottom) {
    *left = GX_GET_PE_REG(8);
    *top = GX_GET_PE_REG(10);
    *right = GX_GET_PE_REG(9);
    *bottom = GX_GET_PE_REG(11);
}
