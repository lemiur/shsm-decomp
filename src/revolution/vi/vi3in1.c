// Reference: zeldaret/tp libs/revolution/src/vi/vi3in1.c
//
// Control of the video DAC (the "3in1" encoder): TV format, VBI data (CGMS, WSS, closed caption), Macrovision,
// gamma, trap filter and RGB overdrive, all written over the I2C bus in i2c.c. The retail SDK keeps only the
// setters the VI interrupt handler runs and __VISetRevolutionModeSimple; the other helpers are inlined into
// them or dead-stripped, and the SDK's "Unkwon TV mode" assertion in __VISetYUVSEL is not in retail.
#include <revolution/os.h>
#include <revolution/vi.h>

#include <revolution/vi/__vi.h>

/* I2C address of the video DAC. */
#define DAC_I2C_ADDR 0xE0

static VIVideo Vdac_Flag_Region;
volatile u32 Vdac_Flag_Changed = 0; // volatile: SDK declaration (zeldaret/tp vi3in1.c); the retrace handler in vi.c polls it
static u32 __current_3in1_video_mode = 0;
static VIACPType __type;
static u32 __tvType = 0xFF;

/* One entry per VIGamma value (index 0 is unused). */
static VIGammaObj gammaSet[] = {
    {{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}},
    {{0x0000, 0x0000, 0x0000, 0x0030, 0x0397, 0x3B49},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1000, 0x1000, 0x1000, 0x1080, 0x1B80, 0xEB00}},
    {{0x0000, 0x0028, 0x005A, 0x02DB, 0x0D8D, 0x3049},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1000, 0x1040, 0x1100, 0x1880, 0x4200, 0xEB00}},
    {{0x0000, 0x007A, 0x023C, 0x076D, 0x129C, 0x2724},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1000, 0x10C0, 0x1580, 0x2900, 0x6200, 0xEB00}},
    {{0x004E, 0x0199, 0x052D, 0x0B24, 0x1429, 0x20A4},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1040, 0x12C0, 0x1DC0, 0x3B00, 0x78C0, 0xEB00}},
    {{0x00EC, 0x03D7, 0x0800, 0x0D9E, 0x143E, 0x1BDB},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x10C0, 0x16C0, 0x27C0, 0x4B80, 0x8980, 0xEB00}},
    {{0x0276, 0x0666, 0x0A96, 0x0EF3, 0x13AC, 0x1849},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1200, 0x1C00, 0x3280, 0x59C0, 0x9600, 0xEB00}},
    {{0x04EC, 0x08F5, 0x0C96, 0x0FCF, 0x12C6, 0x1580},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1400, 0x2200, 0x3CC0, 0x6640, 0x9FC0, 0xEB00}},
    {{0x0800, 0x0BAE, 0x0E00, 0x1030, 0x11CB, 0x1349},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1680, 0x28C0, 0x4680, 0x7100, 0xA780, 0xEB00}},
    {{0x0BB1, 0x0E14, 0x0F2D, 0x1018, 0x10E5, 0x1180},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x1980, 0x2F80, 0x4FC0, 0x7A00, 0xADC0, 0xEB00}},
    {{0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000},
     {0x10, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xEB},
     {0x1000, 0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xEB00}},
    {{0x14EC, 0x11C2, 0x1078, 0x0FB6, 0x0F2F, 0x0EB6},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x2100, 0x3CC0, 0x5FC0, 0x8900, 0xB780, 0xEB00}},
    {{0x19D8, 0x1333, 0x10D2, 0x0F6D, 0x0E5E, 0x0DA4},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x2500, 0x4300, 0x66C0, 0x8F40, 0xBB40, 0xEB00}},
    {{0x1EC4, 0x147A, 0x110F, 0x0F0C, 0x0DA1, 0x0CB6},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x2900, 0x4900, 0x6D40, 0x94C0, 0xBE80, 0xEB00}},
    {{0x2400, 0x1570, 0x110F, 0x0EAA, 0x0D0F, 0x0BDB},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x2D40, 0x4EC0, 0x7300, 0x9980, 0xC180, 0xEB00}},
    {{0x293B, 0x163D, 0x110F, 0x0E30, 0x0C7D, 0x0B24},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x3180, 0x5440, 0x7880, 0x9DC0, 0xC400, 0xEB00}},
    {{0x2E27, 0x170A, 0x10D2, 0x0DE7, 0x0BEB, 0x0A80},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x3580, 0x5980, 0x7D40, 0xA1C0, 0xC640, 0xEB00}},
    {{0x3362, 0x175C, 0x10D2, 0x0D6D, 0x0B6D, 0x09ED},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x39C0, 0x5E40, 0x8200, 0xA540, 0xC840, 0xEB00}},
    {{0x384E, 0x17AE, 0x10B4, 0x0D0C, 0x0AF0, 0x096D},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x3DC0, 0x62C0, 0x8640, 0xA880, 0xCA00, 0xEB00}},
    {{0x3D3B, 0x1800, 0x105A, 0x0CC3, 0x0A72, 0x0900},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x41C0, 0x6740, 0x8A00, 0xAB80, 0xCB80, 0xEB00}},
    {{0x41D8, 0x1828, 0x103C, 0x0C49, 0x0A1F, 0x0892},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x4580, 0x6B40, 0x8DC0, 0xAE00, 0xCD00, 0xEB00}},
    {{0x4676, 0x1851, 0x0FE1, 0x0C00, 0x09B6, 0x0836},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x4940, 0x6F40, 0x9100, 0xB080, 0xCE40, 0xEB00}},
    {{0x4AC4, 0x187A, 0x0FA5, 0x0B9E, 0x0963, 0x07DB},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x4CC0, 0x7300, 0x9440, 0xB2C0, 0xCF80, 0xEB00}},
    {{0x4F13, 0x1851, 0x0F69, 0x0B6D, 0x090F, 0x0780},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x5040, 0x7640, 0x9700, 0xB500, 0xD0C0, 0xEB00}},
    {{0x5313, 0x187A, 0x0F0F, 0x0B24, 0x08BC, 0x0736},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x5380, 0x79C0, 0x99C0, 0xB700, 0xD1C0, 0xEB00}},
    {{0x5713, 0x1851, 0x0EF0, 0x0AC3, 0x087D, 0x06ED},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x56C0, 0x7CC0, 0x9C80, 0xB8C0, 0xD2C0, 0xEB00}},
    {{0x5B13, 0x1828, 0x0E96, 0x0A92, 0x0829, 0x06B6},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x5A00, 0x7FC0, 0x9EC0, 0xBA80, 0xD380, 0xEB00}},
    {{0x5EC4, 0x1800, 0x0E78, 0x0A30, 0x0800, 0x066D},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x5D00, 0x8280, 0xA140, 0xBC00, 0xD480, 0xEB00}},
    {{0x6276, 0x17D7, 0x0E1E, 0x0A00, 0x07C1, 0x0636},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x6000, 0x8540, 0xA340, 0xBD80, 0xD540, 0xEB00}},
    {{0x65D8, 0x17AE, 0x0DE1, 0x09CF, 0x0782, 0x0600},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x62C0, 0x87C0, 0xA540, 0xBF00, 0xD600, 0xEB00}},
    {{0x693B, 0x1785, 0x0DA5, 0x0986, 0x0743, 0x05DB},
     {0x10, 0x1D, 0x36, 0x58, 0x82, 0xB3, 0xEB},
     {0x1000, 0x6580, 0x8A40, 0xA740, 0xC040, 0xD680, 0xEB00}}};

/* Macrovision (ACP) register images, 26 bytes each, per TV format and copy-protection type. */
VIMacroVisionObj VINtscACPType1 = {0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

VIMacroVisionObj VINtscACPType2 = {0x3E, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x00};

VIMacroVisionObj VINtscACPType3 = {0x3E, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x00};

VIMacroVisionObj VIPalACPType1 = {0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00, 0x1C,
                                  0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE, 0x7E, 0x60,
                                  0x00, 0x08, 0x00, 0x04, 0x07, 0x01, 0x55, 0x01};

VIMacroVisionObj VIPalACPType2 = {0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00, 0x1C,
                                  0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE, 0x7E, 0x60,
                                  0x00, 0x08, 0x00, 0x04, 0x07, 0x01, 0x55, 0x01};

VIMacroVisionObj VIPalACPType3 = {0x36, 0x1A, 0x22, 0x2A, 0x22, 0x05, 0x02, 0x00, 0x1C,
                                  0x3D, 0x14, 0x03, 0xFE, 0x01, 0x54, 0xFE, 0x7E, 0x60,
                                  0x00, 0x08, 0x00, 0x04, 0x07, 0x01, 0x55, 0x01};

VIMacroVisionObj VIEurgb60ACPType1 = {0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B,
                                      0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x1E, 0x1E, 0x60,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

VIMacroVisionObj VIEurgb60ACPType2 = {0x36, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00, 0x1B,
                                      0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x1E, 0x1E, 0x60,
                                      0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x01};

VIMacroVisionObj VIEurgb60ACPType3 = {0x36, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02, 0x1B,
                                      0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x1E, 0x1E, 0x60,
                                      0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x01};

VIMacroVisionObj VIMpalACPType1 = {0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

VIMacroVisionObj VIMpalACPType2 = {0x36, 0x1D, 0x11, 0x25, 0x11, 0x01, 0x07, 0x00, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x00};

VIMacroVisionObj VIMpalACPType3 = {0x36, 0x17, 0x15, 0x21, 0x15, 0x05, 0x05, 0x02, 0x1B,
                                   0x1B, 0x24, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0x0F, 0x60,
                                   0x01, 0x0A, 0x00, 0x05, 0x04, 0x03, 0xFF, 0x00};

VIMacroVisionObj VIProgressiveACPType = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

VIMacroVisionObj VIZeroACPType = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* Selects the DAC video mode and the output select bit (inlined into __VISetRGBModeImm). */
static void __VISetVideoMode(VIVideo vmode, VIBool outsel) {
    u8 buffer[2];

    Vdac_Flag_Region = vmode;
    buffer[0] = 1;
    buffer[1] = (u8)((outsel << 5) | Vdac_Flag_Region);
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

/* Enables the closed-caption select (inlined into __VISetRevolutionModeSimple). */
static void __VISetCCSEL(VIBool ccsel) {
    u8 buffer[2];

    buffer[0] = 0x6A;
    buffer[1] = VI_ENABLE;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

/* Enables oversampling (inlined into __VISetRevolutionModeSimple). */
static void __VISetOverSampling(VIBool os) {
    u8 buffer[2];

    buffer[0] = 0x65;
    buffer[1] = VI_ENABLE;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

/* Sets the DAC audio volume (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void __VISetVolume(u8 volumeL, u8 volumeR) {
    u8 buffer[3];

    buffer[0] = 0x71;
    buffer[1] = volumeL;
    buffer[2] = volumeR;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 3);
    WaitMicroTime(2);
}

/*
 * __VISetYUVSEL
 *
 * Picks the DAC video mode from the TV format the boot code left in low memory (the word at offset 0xCC) and
 * writes it with the YUV output select.
 */
void __VISetYUVSEL(VIBool outsel) {
    u8 buffer[2];
    u32 tv;

    tv = *(u32*)OSPhysicalToCached(0xCC);

    switch (tv) {
    case VI_PAL:
    case VI_EURGB60:
        Vdac_Flag_Region = VI_VMODE_PAL;
        break;
    case VI_MPAL:
        Vdac_Flag_Region = VI_VMODE_MPAL;
        break;
    case VI_NTSC:
        Vdac_Flag_Region = VI_VMODE_NTSC;
        break;
    default:
        Vdac_Flag_Region = VI_VMODE_NTSC;
        break;
    }

    buffer[0] = 1;
    buffer[1] = (u8)((outsel << 5) | Vdac_Flag_Region);
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
} // 0x8022B2B0

/* Selects the DAC timing mode (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void __VISetTiming(VITiming timing) {
    u8 buffer[2];

    buffer[0] = 0;
    buffer[1] = timing;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

/*
 * __VISetFilter4EURGB60
 *
 * Switches the EURGB60 output filter.
 */
void __VISetFilter4EURGB60(VIBool enable) {
    u8 buffer[2];

    buffer[0] = 0x6E;
    buffer[1] = (u8)enable;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
} // 0x8022B350

/* Enables or disables the VBI data lines (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void __VISetVBICtrl(VIBool cgms, VIBool wss, VIBool cc) {
    u8 buffer[2];

    buffer[0] = 2;
    buffer[1] = (u8)(((~wss & 1) << 2) | ((~cgms & 1) << 1) | (~cc & 1));
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

static u8 __wd0 = 0xFF;
static u8 __wd1 = 0xFF;
static u8 __wd2 = 0xFF;

/*
 * __VISetCGMS
 *
 * Writes the three CGMS data words to the DAC.
 */
void __VISetCGMS(void) {
    u8 buffer[3];

    buffer[0] = 5;
    buffer[1] = (u8)(((__wd1 & 0xF) << 2) | (__wd0 & 3));
    buffer[2] = __wd2;
    __VISendI2CData(DAC_I2C_ADDR, buffer, 3);
    WaitMicroTime(2);
} // 0x8022B390

/* Clears the CGMS data (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void __VISetCGMSClear(void) {
    __wd0 = 0;
    __wd1 = 0;
    __wd2 = 0;
    __VISetCGMS();
}

/*
 * VISetCGMS
 *
 * Dead-stripped: no text in retail. Its report string (and the __FILE__ string ahead of it) survive in .data,
 * so the SDK reported the forbidden APS trigger in progressive DTV mode here. The rest of the body is unknown, and
 * so is the OSPanic line number: 1 is an unknown placeholder that reaches no retail byte.
 */
void VISetCGMS(void) {
    OSPanic(__FILE__, 1, "VISetCGMS(): Tried to set APS Trigger bit(WORD2[3:2]) \nwhen the scan is progressive in DTV mode, which is forbidden\n");
}

static u8 __gp1 = 0xFF;
static u8 __gp2 = 0xFF;
static u8 __gp3 = 0xFF;
static u8 __gp4 = 0xFF;

/*
 * __VISetWSS
 *
 * Writes the wide screen signalling data to the DAC.
 */
void __VISetWSS(void) {
    u8 buffer[3];

    buffer[0] = 8;
    buffer[1] = (u8)(((__gp2 & 0xF) << 4) | (__gp1 & 0xF));
    buffer[2] = (u8)(((__gp4 & 7) << 3) | (__gp3 & 7));
    __VISendI2CData(DAC_I2C_ADDR, buffer, 3);
    WaitMicroTime(2);
} // 0x8022B3F0

/* Sets the wide screen signalling data (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void VISetWSS(u8 gp1, u8 gp2, u8 gp3, u8 gp4) {
    if ((__gp1 == gp1) && (__gp2 == gp2) && (__gp3 == gp3) && (__gp4 == gp4)) {
        return;
    }

    __gp1 = gp1;
    __gp2 = gp2;
    __gp3 = gp3;
    __gp4 = gp4;

    Vdac_Flag_Changed |= 2;
}

static u8 __cc1 = 0xFF;
static u8 __cc2 = 0xFF;
static u8 __cc3 = 0xFF;
static u8 __cc4 = 0xFF;

/*
 * __VISetClosedCaption
 *
 * Writes the closed-caption bytes to the DAC.
 */
void __VISetClosedCaption(void) {
    u8 buffer[5];

    buffer[0] = 0x7A;
    buffer[1] = (u8)(__cc1 & 0x7F);
    buffer[2] = (u8)(__cc2 & 0x7F);
    buffer[3] = (u8)(__cc3 & 0x7F);
    buffer[4] = (u8)(__cc4 & 0x7F);
    __VISendI2CData(DAC_I2C_ADDR, buffer, 5);
    WaitMicroTime(2);
} // 0x8022B460

/* Sets the closed-caption bytes (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void VISetClosedCaption(u8 cc1, u8 cc2, u8 cc3, u8 cc4) {
    if ((__cc1 == cc1) && (__cc2 == cc2) && (__cc3 == cc3) && (__cc4 == cc4)) {
        return;
    }

    __cc1 = cc1;
    __cc2 = cc2;
    __cc3 = cc3;
    __cc4 = cc4;

    Vdac_Flag_Changed |= 4;
}

/* Writes a Macrovision register image (inlined into its callers; the out-of-line copy is dead-stripped). */
void __VISetMacrovisionImm(VIMacroVisionObj* mparam) {
    u8 i;
    u8 buffer[27];

    buffer[0] = 0x40;

    for (i = 1; i < 27; i++) {
        buffer[i] = mparam->m[i - 1];
    }

    __VISendI2CData(DAC_I2C_ADDR, buffer, 27);
    WaitMicroTime(2);
}

/*
 * __VISetMacrovision
 *
 * Writes the Macrovision image for the current copy-protection type and TV format.
 */
void __VISetMacrovision(void) {
    switch (__type) {
    case VI_ACP_TYPE1:
        switch (__tvType) {
        case VI_NTSC:
            __VISetMacrovisionImm(&VINtscACPType1);
            break;
        case VI_PAL:
            __VISetMacrovisionImm(&VIPalACPType1);
            break;
        case VI_MPAL:
            __VISetMacrovisionImm(&VIMpalACPType1);
            break;
        case VI_EURGB60:
            __VISetMacrovisionImm(&VIEurgb60ACPType1);
            break;
        }
        break;
    case VI_ACP_TYPE2:
        switch (__tvType) {
        case VI_NTSC:
            __VISetMacrovisionImm(&VINtscACPType2);
            break;
        case VI_PAL:
            __VISetMacrovisionImm(&VIPalACPType2);
            break;
        case VI_MPAL:
            __VISetMacrovisionImm(&VIMpalACPType2);
            break;
        case VI_EURGB60:
            __VISetMacrovisionImm(&VIEurgb60ACPType2);
            break;
        }
        break;
    case VI_ACP_TYPE3:
        switch (__tvType) {
        case VI_NTSC:
            __VISetMacrovisionImm(&VINtscACPType3);
            break;
        case VI_PAL:
            __VISetMacrovisionImm(&VIPalACPType3);
            break;
        case VI_MPAL:
            __VISetMacrovisionImm(&VIMpalACPType3);
            break;
        case VI_EURGB60:
            __VISetMacrovisionImm(&VIEurgb60ACPType3);
            break;
        }
        break;
    case VI_ACP_OFF:
        __VISetMacrovisionImm(&VIZeroACPType);
        break;
    }
} // 0x8022B4D0

/*
 * __VISetGammaImm
 *
 * Writes a gamma curve (six coefficients, seven input and seven output points) to the DAC.
 */
void __VISetGammaImm(VIGammaObj* gamma) {
    u8 i, j;
    u8 buffer[34];

    buffer[0] = 0x10;
    i = 1;
    for (j = 0; j < 6; j++) {
        buffer[i] = (u8)((gamma->a[j] >> 8) & 0xFF);
        buffer[i + 1] = (u8)(gamma->a[j] & 0xFF);
        i += 2;
    }

    for (j = 0; j < 7; j++) {
        buffer[i] = gamma->yin[j];
        i++;
    }

    for (j = 0; j < 7; j++) {
        buffer[i] = (u8)((gamma->yout[j] >> 8) & 0xFF);
        buffer[i + 1] = (u8)(gamma->yout[j] & 0xC0);
        i += 2;
    }

    __VISendI2CData(DAC_I2C_ADDR, buffer, 34);
    WaitMicroTime(2);
} // 0x8022C250

/*
 * __VISetGamma1_0
 *
 * Writes the neutral (1.0) gamma curve.
 */
void __VISetGamma1_0(void) {
    __VISetGammaImm(&gammaSet[VI_GM_1_0]);
} // 0x8022C3B0

static VIGamma __gamma;

/*
 * __VISetGamma
 *
 * Writes the gamma curve chosen with VISetGamma.
 */
void __VISetGamma(void) {
    __VISetGammaImm(&gammaSet[__gamma]);
} // 0x8022C3C0

/*
 * VISetGamma
 *
 * Selects the gamma curve and flags it for the next retrace.
 */
void VISetGamma(VIGamma gamma) {
    if (__gamma != gamma) {
        __gamma = gamma;
        Vdac_Flag_Changed |= 0x10;
    }
} // 0x8022C3E0

/* Switches the trap filter at once (inlined into __VISetRevolutionModeSimple; the out-of-line copy is dead-stripped). */
void __VISetTrapFilterImm(VIBool filter) {
    u8 buffer[2];

    buffer[0] = 3;

    if (filter == VI_TRUE) {
        buffer[1] = VI_FALSE;
    } else {
        buffer[1] = VI_TRUE;
    }
    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
}

static VIBool __filter = 0xFF;

/*
 * __VISetTrapFilter
 *
 * Writes the trap filter state chosen with VISetTrapFilter.
 */
void __VISetTrapFilter(void) {
    u8 buffer[2];

    buffer[0] = 3;

    if (__filter == VI_TRUE) {
        buffer[1] = VI_FALSE;
    } else {
        buffer[1] = VI_TRUE;
    }

    __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
    WaitMicroTime(2);
} // 0x8022C400

/* Selects the trap filter state and flags it for the next retrace (dead-stripped: no retail caller). */
void VISetTrapFilter(VIBool filter) {
    if (__filter == filter) {
        return;
    }

    __filter = filter;
    Vdac_Flag_Changed |= 0x20;
}

static VIOverDrive __level;

/*
 * __VISetRGBOverDrive
 *
 * Writes the RGB overdrive level, which only applies in the RGB video mode.
 */
void __VISetRGBOverDrive(void) {
    u8 buffer[2];

    if (Vdac_Flag_Region == VI_VMODE_RGB) {
        buffer[0] = 0xA;
        buffer[1] = (u8)((__level << 1) | VI_ENABLE);
        __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
        WaitMicroTime(2);
    } else {
        buffer[0] = 0xA;
        buffer[1] = VI_DISABLE;
        __VISendI2CData(DAC_I2C_ADDR, buffer, 2);
        WaitMicroTime(2);
    }
} // 0x8022C460

/* Selects the RGB overdrive level and flags it for the next retrace (inlined into __VISetRevolutionModeSimple). */
void VISetRGBOverDrive(VIOverDrive level) {
    if (__level == level) {
        return;
    }

    __level = level;
    Vdac_Flag_Changed |= 0x40;
}

/*
 * VISetRGBModeImm
 *
 * Flags the switch to the RGB video mode for the next retrace.
 */
void VISetRGBModeImm(void) {
    Vdac_Flag_Changed |= 0x80;
} // 0x8022C4F0

/*
 * __VISetRGBModeImm
 *
 * Switches the DAC to the RGB video mode at once.
 */
void __VISetRGBModeImm(void) {
    __VISetVideoMode(VI_VMODE_RGB, VI_DISABLE);
} // 0x8022C500

/*
 * __VISetRevolutionModeSimple
 *
 * Programs the DAC for the Wii's default output: closed-caption select and oversampling on, game timing, no
 * VBI data, Macrovision off, neutral gamma.
 */
void __VISetRevolutionModeSimple(void) {
    u32 dtv;

    __VISetCCSEL(VI_TRUE);
    __VISetOverSampling(VI_TRUE);
    dtv = VIGetDTVStatus();
    __VISetYUVSEL((VIBool)dtv);
    __VISetTiming(VI_TMG_GAME);
    __VISetVolume(0x8E, 0x8E);
    __VISetVBICtrl(VI_DISABLE, VI_DISABLE, VI_DISABLE);
    __VISetCGMSClear();
    VISetWSS(0, 0, 0, 0);
    __VISetWSS();
    VISetClosedCaption(0, 0, 0, 0);
    __VISetClosedCaption();
    __VISetMacrovisionImm(&VIZeroACPType);
    VISetRGBOverDrive(VI_ODV_L1);
    __VISetRGBOverDrive();
    __VISetTrapFilterImm(VI_DISABLE);
    __VISetGammaImm(&gammaSet[VI_GM_1_0]);
    __current_3in1_video_mode = 0;
} // 0x8022C550
