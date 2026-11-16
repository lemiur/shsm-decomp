// Reference: doldecomp/ogws include/revolution/AX/{AXAlloc,AXAux,AXCL,AXOut,AXVPB,AXSPB}.h
//
// AX-internal declarations shared by the AX units. Each function and global is declared once, here or in ax.h.
#ifndef REVOLUTION_AX_INTERNAL_H
#define REVOLUTION_AX_INTERNAL_H

#include <revolution/types.h>
#include <revolution/ax.h>

#ifdef __cplusplus
extern "C" {
#endif

// AX (the Quit functions are named after the Init ones: invented names)
void __AXAllocQuit(void);
void __AXAuxQuit(void);
void __AXClQuit(void);
void __AXOutQuit(void);
void __AXSPBQuit(void);
void __AXVPBQuit(void);

// AXAlloc
AXVPB* __AXGetStackHead(u32 prio);
void __AXServiceCallbackStack(void);
void __AXInitVoiceStacks(void);
void __AXAllocInit(void);
void __AXPushFreeStack(AXVPB* vpb);
AXVPB* __AXPopFreeStack(void);
void __AXPushCallbackStack(AXVPB* vpb);
AXVPB* __AXPopCallbackStack(void);
void __AXRemoveFromStack(AXVPB* vpb);
void __AXPushStackHead(AXVPB* vpb, u32 prio);
AXVPB* __AXPopStackFromBottom(u32 prio);

// AXAux
void __AXAuxInit(void);
void __AXGetAuxAInput(void** out);
void __AXGetAuxAOutput(void** out);
void __AXGetAuxAInputDpl2(void** out);
void __AXGetAuxAOutputDpl2R(void** out);
void __AXGetAuxAOutputDpl2Ls(void** out);
void __AXGetAuxAOutputDpl2Rs(void** out);
void __AXGetAuxBInput(void** out);
void __AXGetAuxBOutput(void** out);
void __AXGetAuxBInputDpl2(void** out);
void __AXGetAuxBOutputDpl2R(void** out);
void __AXGetAuxBOutputDpl2Ls(void** out);
void __AXGetAuxBOutputDpl2Rs(void** out);
void __AXGetAuxCInput(void** out);
void __AXGetAuxCOutput(void** out);
void __AXProcessAux(void);

// AXCL
extern u32 __AXClMode;
u32 __AXGetCommandListCycles(void);
void* __AXGetCommandListAddress(void);
void __AXNextFrame(void* surround, void* lr, void** rmt);
void __AXClInit(void);

// AXOut
extern AXPROFILE __AXLocalProfile;
u32 __AXOutNewFrame(void);
void __AXOutAiCallback(void);
void __AXOutInitDSP(void);
void __AXOutInit(u32 mode);

// AXComp (the compressor curve the DSP program reads)
extern u16 __AXCompressorTable[];

// AXSPB: the studio block holds the depop levels the DSP program fades out of each mix bus.
#pragma pack(push, 1)
typedef struct _AXSTUDIO {
    s32 L;
    s16 dL;

    s32 R;
    s16 dR;

    s32 S;
    s16 dS;

    s32 AuxAL;
    s16 dAuxAL;

    s32 AuxAR;
    s16 dAuxAR;

    s32 AuxAS;
    s16 dAuxAS;

    s32 AuxBL;
    s16 dAuxBL;

    s32 AuxBR;
    s16 dAuxBR;

    s32 AuxBS;
    s16 dAuxBS;

    s32 AuxCL;
    s16 dAuxCL;

    s32 AuxCR;
    s16 dAuxCR;

    s32 AuxCS;
    s16 dAuxCS;

    s32 Main0;
    s16 dMain0;

    s32 Aux0;
    s16 dAux0;

    s32 Main1;
    s16 dMain1;

    s32 Aux1;
    s16 dAux1;

    s32 Main2;
    s16 dMain2;

    s32 Aux2;
    s16 dAux2;

    s32 Main3;
    s16 dMain3;

    s32 Aux3;
    s16 dAux3;
} AXSTUDIO;
#pragma pack(pop)

AXSTUDIO* __AXGetStudio(void);
void __AXDepopFadeMain(s32* all, s32* value, s16* depop);
void __AXDepopFadeRmt(s32* all, s32* value, s16* depop);
void __AXPrintStudio(void);
void __AXSPBInit(void);
void __AXDepopVoice(AXPB* pb);

// AXProf
AXPROFILE* __AXGetCurrentProfile(void);

// AXComp (the DSP program the output unit loads)
extern u8 axDspSlave[];
extern u16 axDspSlaveLength;
extern u16 axDspInitVector;
extern u16 axDspResumeVector;

// AXVPB
s32 __AXGetNumVoices(void);
void __AXServiceVPB(AXVPB* vpb);
void __AXDumpVPB(AXVPB* vpb);
void __AXSyncPBs(u32 baseCycles);
void __AXVPBInit(void);
AXPB* __AXGetPBs(void);
void __AXSetPBDefault(AXVPB* vpb);
extern s32 __AXMaxVoices;

#ifdef __cplusplus
}
#endif

#endif
