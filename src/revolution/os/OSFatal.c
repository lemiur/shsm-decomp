// Reference: zeldaret/tp libs/revolution/src/os/OSFatal.c
#include <revolution/base.h>
#include <revolution/os.h>
#include <revolution/exi.h>
#include <revolution/hardware.h>
#include <revolution/vi.h>
#include <string.h>

/* Low-memory pointer to the file system table when the boot info does not carry one. */
#define OS_FST_LOCATION_ADDR 0x3110

/* Arena base used by the fatal-error screen. */
#define OS_FATAL_ARENA_LO ((void*)0x81400000)

/* Size of the font buffer and the YUV frame buffer (640 x 480 x 2 bytes). */
#define OS_FATAL_FONT_SIZE 0xA1004
#define OS_FATAL_XFB_SIZE 0x96000

#define OS_FATAL_XFB_WIDTH 640
#define OS_FATAL_XFB_HEIGHT 480

typedef struct OSFatalParam {
    GXColor fg;
    GXColor bg;
    const char* msg;
} OSFatalParam;

static OSFatalParam FatalParam;
static OSContext FatalContext;

static void Halt(void);

/* Fills the xfbW x xfbH YUV frame buffer xfb with the colour yuv. */
static void ScreenClear(void* xfb, u16 xfbW, u16 xfbH, GXColor yuv) {
    int i;
    int j;
    u8* ptr;

    ptr = xfb;
    for (i = 0; i < xfbH; i++) {
        for (j = 0; j < xfbW; j += 2) {
            *ptr++ = yuv.r;
            *ptr++ = yuv.g;
            *ptr++ = yuv.r;
            *ptr++ = yuv.b;
        }
    }
}

/*
 * ScreenReport
 *
 * Draws string on the frame buffer, one glyph at a time from the ROM font, in
 * the colour yuv. Text starts at (x, y), wraps or breaks lines leading rows
 * further down, and stops at the bottom of the screen.
 */
static void ScreenReport(void* xfb, u16 xfbW, u16 xfbH, GXColor yuv, s32 x, s32 y, s32 leading, const char* string) {
    u8* ptr;
    s32 width;
    u32 i;
    u32 j;
    u32 image[72];
    u32 k;
    u32 l;
    u8 Y;
    u32 pixel;
    s32 col;

new_line:
    if (xfbH - 24 >= y) {
        ptr = (u8*)xfb + ((x + (y * xfbW)) * 2);
        col = x;

        while ((s8)*string != 0) {
            if ((s8)*string == '\n') {
                string++;
                y += leading;
                goto new_line;
            }

            if (xfbW - 48 < col) {
                y += leading;
                goto new_line;
            }

            for (i = 0; i < 24; i++) {
                j = (i & 7) + ((i >> 3) * 24);
                image[j + 0] = 0;
                image[j + 8] = 0;
                image[j + 16] = 0;
            }

            string = OSGetFontTexel(string, image, 0, 6, &width);

            for (i = 0; i < 24; i++) {
                j = (i & 7) + ((i >> 3) * 24);

                for (k = 0; k < 24; k++) {
                    l = j + 8 * (k / 8);

                    Y = (image[l] >> ((7 - (k & 7)) * 4)) & 0xF;
                    if (Y != 0) {
                        Y = (((yuv.r * (Y * 0xEF)) / 255) / 15) + 0x10;
                        pixel = k + (i * xfbW);
                        ptr[pixel * 2] = Y;

                        if ((col + k) & 1) {
                            ptr[(pixel * 2) - 1] = yuv.g;
                            ptr[(pixel * 2) + 1] = yuv.b;
                        } else {
                            ptr[(pixel * 2) - 1] = yuv.b;
                            ptr[(pixel * 2) + 1] = yuv.g;
                        }
                    }
                }
            }

            ptr += width * 2;
            col += width;
        }
    }
} // 0x80215BC0

/* Sets up the video interface for a 640 x 480 frame buffer in the console's TV format. */
static void ConfigureVideo(u16 xfbW, u16 xfbH) {
    GXRenderModeObj mode;
    mode.fbWidth = xfbW;
    mode.efbHeight = 480;
    mode.xfbHeight = xfbH;
    mode.viXOrigin = 40;
    mode.viWidth = 640;
    mode.viHeight = xfbH;

    switch (VIGetTvFormat()) {
    case VI_NTSC:
    case VI_MPAL:
        if (__VIRegs[54] & 1) {
            mode.viTVmode = 2;
            mode.viYOrigin = 0;
            mode.xFBmode = 0;
        } else {
            mode.viTVmode = 0;
            mode.viYOrigin = 0;
            mode.xFBmode = 1;
        }
        break;
    case VI_EURGB60:
        if (__VIRegs[54] & 1) {
            mode.viTVmode = 22;
            mode.viYOrigin = 0;
            mode.xFBmode = 0;
        } else {
            mode.viTVmode = 20;
            mode.viYOrigin = 0;
            mode.xFBmode = 1;
        }
        break;
    case VI_PAL:
        mode.viTVmode = 4;
        mode.viYOrigin = 47;
        mode.xFBmode = 1;
        break;
    }

    VIConfigure(&mode);
    VIConfigurePan(0, 0, 640, 480);
} // 0x80215EF0

/* Converts rgb to the YUV colour the frame buffer holds. */
static GXColor RGB2YUV(GXColor rgb) {
    f32 Y;
    f32 Cb;
    f32 Cr;
    GXColor yuv;

    Y = 0.5f + (16.0f + ((0.098f * (f32)rgb.b) + ((0.257f * (f32)rgb.r) + (0.504f * (f32)rgb.g))));
    Cb = 0.5f + (128.0f + ((0.439f * (f32)rgb.b) + ((-0.148f * (f32)rgb.r) - (0.291f * (f32)rgb.g))));
    Cr = 0.5f + (128.0f + (((0.439f * (f32)rgb.r) - (0.368f * (f32)rgb.g)) - (0.071f * (f32)rgb.b)));

    yuv.r = (Y > 235.0f) ? 235.0f : (Y < 16.0f) ? 16.0f : Y;
    yuv.g = (Cb > 240.0f) ? 240.0f : (Cb < 16.0f) ? 16.0f : Cb;
    yuv.b = (Cr > 240.0f) ? 240.0f : (Cr < 16.0f) ? 16.0f : Cr;
    yuv.a = 0;

    return yuv;
}

/*
 * OSFatal
 *
 * Shuts the system down as far as the screen goes, then continues in Halt on a
 * fresh stack at the top of the arena to show msg and stop.
 */
void OSFatal(GXColor fg, GXColor bg, const char* msg) {
    OSBootInfo* bootInfo;
    u32 count;
    OSTime t;

    bootInfo = (OSBootInfo*)OSPhysicalToCached(0);
    OSDisableInterrupts();
    OSDisableScheduler();
    OSClearContext(&FatalContext);
    OSSetCurrentContext(&FatalContext);
    __OSStopAudioSystem();
    VIInit();
    __OSUnmaskInterrupts(0x80);
    VISetBlack(TRUE);
    VIFlush();
    VISetPreRetraceCallback(NULL);
    VISetPostRetraceCallback(NULL);
    OSEnableInterrupts();

    count = VIGetRetraceCount();
    do {} while ((s32)(VIGetRetraceCount() - count) < 1);

    t = OSGetTime();
    while (!__OSCallShutdownFunctions(FALSE, 0) && OSGetTime() - t < OSMillisecondsToTicks(1000)) {}

    OSDisableInterrupts();
    __OSCallShutdownFunctions(TRUE, 0);
    EXISetExiCallback(0, NULL);
    EXISetExiCallback(2, NULL);

    while (!EXILock(0, 1, NULL)) {
        EXISync(0);
        EXIDeselect(0);
        EXIUnlock(0);
    }
    EXIUnlock(0);

    do {} while ((__EXIRegs[3] & 1) == 1);

    __OSSetExceptionHandler(8, OSDefaultExceptionHandler);
    GXAbortFrame();
    OSSetArenaLo(OS_FATAL_ARENA_LO);

    if (!bootInfo->FSTLocation) {
        OSSetArenaHi(*(void**)OSPhysicalToCached(OS_FST_LOCATION_ADDR));
    } else {
        OSSetArenaHi(bootInfo->FSTLocation);
    }

    FatalParam.fg = fg;
    FatalParam.bg = bg;
    FatalParam.msg = msg;
    OSSwitchFiber((u32)&Halt, (u32)OSGetArenaHi());
} // 0x80216010

/* Runs on the new stack: shows the message from FatalParam, then halts the CPU. */
static void Halt(void) {
    u32 count;
    OSFontHeader* fontData;
    void* xfb;
    u32 len;
    OSFatalParam* fp;

    OSEnableInterrupts();
    fp = &FatalParam;
    len = strlen(fp->msg) + 1;
    fp->msg = memmove(OSAllocFromMEM1ArenaLo(len, 32), fp->msg, len);

    fontData = OSAllocFromMEM1ArenaLo(OS_FATAL_FONT_SIZE, 32);
    OSLoadFont(fontData, OSGetArenaLo());

    xfb = OSAllocFromMEM1ArenaLo(OS_FATAL_XFB_SIZE, 32);
    ScreenClear(xfb, OS_FATAL_XFB_WIDTH, OS_FATAL_XFB_HEIGHT, RGB2YUV(fp->bg));
    VISetNextFrameBuffer(xfb);
    ConfigureVideo(OS_FATAL_XFB_WIDTH, OS_FATAL_XFB_HEIGHT);
    VIFlush();

    count = VIGetRetraceCount();
    do {} while ((s32)(VIGetRetraceCount() - count) < 2);

    ScreenReport(xfb, OS_FATAL_XFB_WIDTH, OS_FATAL_XFB_HEIGHT, RGB2YUV(fp->fg), 48, 100, fontData->leading, fp->msg);
    DCFlushRange(xfb, OS_FATAL_XFB_SIZE);
    VISetBlack(FALSE);
    VIFlush();

    count = VIGetRetraceCount();
    do {} while ((s32)(VIGetRetraceCount() - count) < 1);

    OSDisableInterrupts();
    OSReport("%s\n", fp->msg);
    PPCHalt();
} // 0x80216200
