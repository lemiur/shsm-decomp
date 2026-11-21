// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <climax/unk_80008828.h>
#include <climax/RenderState.h>

// Resets the blend to the additive ONE/ONE state that modes 0, 9 and 10 start from.
static void ResetBlend(void) {
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
}

// Applies the fog colour the current blend mode asked for, when fog is on.
static void ApplyFogColour(void) {
    if (lbl_8052A6E4) {
        switch (lbl_8052A6E8) {
        case 0:
            RwRenderStateSet(rwRENDERSTATEFOGCOLOR, (void*)lbl_8052A6E0);
            break;
        case 1:
            RwRenderStateSet(rwRENDERSTATEFOGCOLOR, (void*)0);
            break;
        case 2:
            RwRenderStateSet(rwRENDERSTATEFOGCOLOR, (void*)0xFFFFFFFF); // white
            break;
        case 3:
            RwRenderStateSet(rwRENDERSTATEFOGCOLOR, (void*)0x80808080); // grey
            break;
        }
    }
}

// Sets the blend mode the renderer draws with (0 to 12): the RenderWare render states and the GX
// blend that go with it, then the fog colour the mode asks for. Mode 0 re-applies the states of the
// mode that was current and leaves that mode current. Name invented.
// 0x80008828
extern "C" void fn_80008828(u32 mode) {
    u32 prev = lbl_80528868;
    if (mode == prev) {
        return;
    }
    if (prev - 9 <= 1 || prev == 0) {
        ResetBlend();
    }
    switch (mode) {
    case 0: {
        // Mode 0 re-applies the states of the mode that was current and leaves that mode current.
        u32 old = lbl_80528868;
        lbl_80528868 = mode;
        if (old != mode) {
            if (mode - 9 <= 1 || mode == 0) {
                ResetBlend();
            }
            switch (old) {
            case 0: {
                u32 m = lbl_80528868;
                lbl_80528868 = old;
                fn_80008828(m);
                goto done;
            }
            case 1:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 0;
                break;
            case 2:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
                lbl_8052A6E8 = 0;
                break;
            case 3:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDZERO);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 0;
                break;
            case 4:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0xC0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
                lbl_8052A6E8 = 0;
                break;
            case 5:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
                lbl_8052A6E8 = 1;
                break;
            case 6:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 1;
                break;
            case 7:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
                lbl_8052A6E8 = 1;
                break;
            case 8:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTALPHA);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 1;
                break;
            case 9:
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
                lbl_8052A6E8 = 1;
                break;
            case 10:
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
                GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_NOOP);
                lbl_8052A6E8 = 1;
                break;
            case 11:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTCOLOR);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 2;
                break;
            case 12:
                RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTCOLOR);
                RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDSRCCOLOR);
                RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
                RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
                lbl_8052A6E8 = 3;
                break;
            default:
                break;
            }
            lbl_80528868 = old;
            ApplyFogColour();
        }
    done:
        // The restore path (and the unchanged-mode path) leaves the fog colour at the scene default.
        lbl_8052A6E8 = 0;
        return;
    }
    case 1:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 0;
        break;
    case 2:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
        lbl_8052A6E8 = 0;
        break;
    case 3:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDZERO);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 0;
        break;
    case 4:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0xC0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
        lbl_8052A6E8 = 0;
        break;
    case 5:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
        lbl_8052A6E8 = 1;
        break;
    case 6:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 1;
        break;
    case 7:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
        lbl_8052A6E8 = 1;
        break;
    case 8:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 1;
        break;
    case 9:
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDONE);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
        lbl_8052A6E8 = 1;
        break;
    case 10:
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, (void*)0);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONGREATER);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
        GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_NOOP);
        lbl_8052A6E8 = 1;
        break;
    case 11:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTCOLOR);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDZERO);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 2;
        break;
    case 12:
        RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDDESTCOLOR);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDSRCCOLOR);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
        RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, (void*)rwALPHATESTFUNCTIONALWAYS);
        lbl_8052A6E8 = 3;
        break;
    }
    lbl_80528868 = mode;
    ApplyFogColour();
}
