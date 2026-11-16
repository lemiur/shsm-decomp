// Reference: zeldaret/tp libs/revolution/src/si/SISamplingRate.c
#include <revolution/os.h>
#include <revolution/hardware.h>
#include <revolution/si.h>
#include <revolution/vi.h>

/* Index of the clock select register in __VIRegs; bit 0 is set in progressive scan. */
#define VI_CLOCK_SEL 54

static u32 SamplingRate = 0;

/* Polling position for one sampling rate: the line the poll starts on and how many polls fit a frame. */
typedef struct XY {
    u16 line;
    u8 count;
} XY;

static XY XYNTSC[12] = {
    {0x00F6, 0x02},
    {0x000E, 0x13},
    {0x001E, 0x09},
    {0x002C, 0x06},
    {0x0034, 0x05},
    {0x0041, 0x04},
    {0x0057, 0x03},
    {0x0057, 0x03},
    {0x0057, 0x03},
    {0x0083, 0x02},
    {0x0083, 0x02},
    {0x0083, 0x02},
};

static XY XYPAL[12] = {
    {0x0128, 0x02},
    {0x000F, 0x15},
    {0x001D, 0x0B},
    {0x002D, 0x07},
    {0x0034, 0x06},
    {0x003F, 0x05},
    {0x004E, 0x04},
    {0x0068, 0x03},
    {0x0068, 0x03},
    {0x0068, 0x03},
    {0x0068, 0x03},
    {0x009C, 0x02},
};

void SISetSamplingRate(u32 msec) {
    XY* xy;
    BOOL progressive;
    BOOL enabled;

    if (msec > 11) {
        msec = 11;
    }
    enabled = OSDisableInterrupts();
    SamplingRate = msec;

    switch (VIGetTvFormat()) {
    case VI_NTSC:
    case VI_MPAL:
    case VI_EURGB60:
        xy = XYNTSC;
        break;
    case VI_PAL:
        xy = XYPAL;
        break;
    default:
        OSReport("SISetSamplingRate: unknown TV format. Use default.");
        msec = 0;
        xy = XYNTSC;
        break;
    }

    progressive = __VIRegs[VI_CLOCK_SEL] & 1;
    SISetXY((progressive ? 2 : 1) * xy[msec].line, xy[msec].count);
    OSRestoreInterrupts(enabled);
} // 0x80210590

void SIRefreshSamplingRate(void) {
    SISetSamplingRate(SamplingRate);
} // 0x80210670
