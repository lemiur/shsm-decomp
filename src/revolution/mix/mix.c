// Reference: zeldaret/tp libs/dolphin/src/mix/mix.c (the GameCube MIX; the Wii retail build adds the auxC bus, the
// 96-voice channel tables, the DPL2 slot assignment and the remote-speaker channel table, which come from the bytes)
//
// Unit name and path unconfirmed: the range was listed as AXART, but it is the MIX mixer (tp's mix.c has the same
// pan, DPL2 and volume tables and the same MIXChannel layout). The remote-speaker half is the unit mix_rmt.c: retail
// calls __MIXGetVolume, __MIXResetRmtChannel and __MIXUpdateRmtChannel out of line across that boundary (inside one
// unit `-ipa file` inlines all of them), so __MIXGetVolume is a global here and the other two are mix_rmt.c's.
#include <revolution/ax.h>
#include <revolution/mix.h>
#include <revolution/mix/__mix.h>
#include <revolution/os.h>
#include <string.h>

/* Voice mode flags: the low bits are set by the caller (MIXInitChannel), the top bits are the update state. */
#define MIX_MODE_AUXA_PRE 0x1
#define MIX_MODE_AUXB_PRE 0x2
#define MIX_MODE_AUXC_PRE 0x4
#define MIX_MODE_MUTE 0x8
#define MIX_MODE_MASK 0xF
#define MIX_FLAG_INPUT_DIRTY 0x10000000
#define MIX_FLAG_INPUT_APPLY 0x20000000

/* AX mixer control bits (pb.mixerCtrl): a voice's volume in a bus is on, and its ramp delta is on. */
#define MIX_CTRL_L 0x00000001
#define MIX_CTRL_R 0x00000002
#define MIX_CTRL_S 0x00000008
#define MIX_CTRL_AL 0x00010000
#define MIX_CTRL_AR 0x00020000
#define MIX_CTRL_AS 0x00080000
#define MIX_CTRL_BL 0x00200000
#define MIX_CTRL_BR 0x00400000
#define MIX_CTRL_BS 0x01000000
#define MIX_CTRL_CL 0x04000000
#define MIX_CTRL_CR 0x08000000
#define MIX_CTRL_CS 0x20000000
#define MIX_CTRL_DELTA_L 0x00000004
#define MIX_CTRL_DELTA_S 0x00000010
#define MIX_CTRL_DELTA_A 0x00040000
#define MIX_CTRL_DELTA_AS 0x00100000
#define MIX_CTRL_DELTA_B 0x00800000
#define MIX_CTRL_DELTA_BS 0x02000000
#define MIX_CTRL_DELTA_C 0x10000000
#define MIX_CTRL_DELTA_CS 0x40000000
#define MIX_CTRL_DPL2 0x80000000

static u16 __MIXVolumeTable[965] = {
        0,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     2,     2,
        2,     2,     2,     2,     2,     2,     2,     2,
        2,     2,     2,     2,     2,     2,     2,     2,
        2,     2,     2,     2,     2,     2,     2,     2,
        2,     2,     2,     2,     2,     2,     2,     2,
        2,     3,     3,     3,     3,     3,     3,     3,
        3,     3,     3,     3,     3,     3,     3,     3,
        3,     3,     3,     3,     3,     3,     3,     3,
        3,     3,     4,     4,     4,     4,     4,     4,
        4,     4,     4,     4,     4,     4,     4,     4,
        4,     4,     4,     4,     4,     5,     5,     5,
        5,     5,     5,     5,     5,     5,     5,     5,
        5,     5,     5,     5,     5,     6,     6,     6,
        6,     6,     6,     6,     6,     6,     6,     6,
        6,     6,     7,     7,     7,     7,     7,     7,
        7,     7,     7,     7,     7,     7,     8,     8,
        8,     8,     8,     8,     8,     8,     8,     8,
        9,     9,     9,     9,     9,     9,     9,     9,
        9,    10,    10,    10,    10,    10,    10,    10,
       10,    10,    11,    11,    11,    11,    11,    11,
       11,    12,    12,    12,    12,    12,    12,    12,
       13,    13,    13,    13,    13,    13,    13,    14,
       14,    14,    14,    14,    14,    15,    15,    15,
       15,    15,    16,    16,    16,    16,    16,    17,
       17,    17,    17,    17,    18,    18,    18,    18,
       18,    19,    19,    19,    19,    19,    20,    20,
       20,    20,    21,    21,    21,    21,    22,    22,
       22,    22,    23,    23,    23,    24,    24,    24,
       24,    25,    25,    25,    26,    26,    26,    26,
       27,    27,    27,    28,    28,    28,    29,    29,
       29,    30,    30,    30,    31,    31,    32,    32,
       32,    33,    33,    33,    34,    34,    35,    35,
       35,    36,    36,    37,    37,    38,    38,    38,
       39,    39,    40,    40,    41,    41,    42,    42,
       43,    43,    44,    44,    45,    45,    46,    46,
       47,    47,    48,    49,    49,    50,    50,    51,
       51,    52,    53,    53,    54,    55,    55,    56,
       56,    57,    58,    58,    59,    60,    61,    61,
       62,    63,    63,    64,    65,    66,    66,    67,
       68,    69,    70,    70,    71,    72,    73,    74,
       75,    75,    76,    77,    78,    79,    80,    81,
       82,    83,    84,    85,    86,    87,    88,    89,
       90,    91,    92,    93,    94,    95,    96,    97,
       98,   100,   101,   102,   103,   104,   106,   107,
      108,   109,   111,   112,   113,   114,   116,   117,
      118,   120,   121,   123,   124,   126,   127,   128,
      130,   131,   133,   135,   136,   138,   139,   141,
      143,   144,   146,   148,   149,   151,   153,   155,
      156,   158,   160,   162,   164,   166,   168,   170,
      171,   173,   175,   178,   180,   182,   184,   186,
      188,   190,   192,   195,   197,   199,   202,   204,
      206,   209,   211,   214,   216,   219,   221,   224,
      226,   229,   231,   234,   237,   240,   242,   245,
      248,   251,   254,   257,   260,   263,   266,   269,
      272,   275,   278,   282,   285,   288,   292,   295,
      298,   302,   305,   309,   312,   316,   320,   323,
      327,   331,   335,   339,   343,   347,   351,   355,
      359,   363,   367,   371,   376,   380,   384,   389,
      393,   398,   403,   407,   412,   417,   422,   427,
      431,   436,   442,   447,   452,   457,   462,   468,
      473,   479,   484,   490,   495,   501,   507,   513,
      519,   525,   531,   537,   543,   550,   556,   562,
      569,   576,   582,   589,   596,   603,   610,   617,
      624,   631,   638,   646,   653,   661,   669,   676,
      684,   692,   700,   708,   716,   725,   733,   742,
      750,   759,   768,   777,   786,   795,   804,   813,
      823,   832,   842,   852,   861,   871,   881,   892,
      902,   912,   923,   934,   945,   955,   967,   978,
      989,  1001,  1012,  1024,  1036,  1048,  1060,  1072,
     1085,  1097,  1110,  1123,  1136,  1149,  1162,  1176,
     1189,  1203,  1217,  1231,  1245,  1260,  1274,  1289,
     1304,  1319,  1334,  1350,  1365,  1381,  1397,  1414,
     1430,  1446,  1463,  1480,  1497,  1515,  1532,  1550,
     1568,  1586,  1604,  1623,  1642,  1661,  1680,  1700,
     1719,  1739,  1759,  1780,  1800,  1821,  1842,  1864,
     1885,  1907,  1929,  1951,  1974,  1997,  2020,  2043,
     2067,  2091,  2115,  2140,  2164,  2190,  2215,  2241,
     2266,  2293,  2319,  2346,  2373,  2401,  2429,  2457,
     2485,  2514,  2543,  2573,  2602,  2632,  2663,  2694,
     2725,  2757,  2789,  2821,  2853,  2887,  2920,  2954,
     2988,  3023,  3058,  3093,  3129,  3165,  3202,  3239,
     3276,  3314,  3353,  3391,  3431,  3470,  3511,  3551,
     3592,  3634,  3676,  3719,  3762,  3805,  3849,  3894,
     3939,  3985,  4031,  4078,  4125,  4173,  4221,  4270,
     4319,  4369,  4420,  4471,  4523,  4575,  4628,  4682,
     4736,  4791,  4846,  4902,  4959,  5017,  5075,  5133,
     5193,  5253,  5314,  5375,  5438,  5501,  5564,  5629,
     5694,  5760,  5827,  5894,  5962,  6031,  6101,  6172,
     6243,  6316,  6389,  6463,  6538,  6613,  6690,  6767,
     6846,  6925,  7005,  7086,  7168,  7251,  7335,  7420,
     7506,  7593,  7681,  7770,  7860,  7951,  8043,  8136,
     8230,  8326,  8422,  8520,  8618,  8718,  8819,  8921,
     9025,  9129,  9235,  9342,  9450,  9559,  9670,  9782,
     9895, 10010, 10126, 10243, 10362, 10482, 10603, 10726,
    10850, 10976, 11103, 11231, 11361, 11493, 11626, 11761,
    11897, 12035, 12174, 12315, 12458, 12602, 12748, 12895,
    13045, 13196, 13349, 13503, 13659, 13818, 13978, 14140,
    14303, 14469, 14636, 14806, 14977, 15151, 15326, 15504,
    15683, 15865, 16049, 16234, 16422, 16613, 16805, 17000,
    17196, 17396, 17597, 17801, 18007, 18215, 18426, 18640,
    18856, 19074, 19295, 19518, 19744, 19973, 20204, 20438,
    20675, 20914, 21156, 21401, 21649, 21900, 22153, 22410,
    22669, 22932, 23197, 23466, 23738, 24013, 24291, 24572,
    24857, 25144, 25436, 25730, 26028, 26329, 26634, 26943,
    27255, 27570, 27890, 28213, 28539, 28870, 29204, 29542,
    29884, 30230, 30580, 30934, 31293, 31655, 32022, 32392,
    32767, 33147, 33531, 33919, 34312, 34709, 35111, 35518,
    35929, 36345, 36766, 37192, 37622, 38058, 38499, 38944,
    39395, 39851, 40313, 40780, 41252, 41730, 42213, 42702,
    43196, 43696, 44202, 44714, 45232, 45756, 46286, 46821,
    47364, 47912, 48467, 49028, 49596, 50170, 50751, 51339,
    51933, 52535, 53143, 53758, 54381, 55011, 55648, 56292,
    56944, 57603, 58270, 58945, 59627, 60318, 61016, 61723,
    62438, 63161, 63892, 64632, 65380
};
static int __MIXPanTable[128] = {
        0,     0,    -1,    -1,    -1,    -2,    -2,    -2,
       -3,    -3,    -4,    -4,    -4,    -5,    -5,    -5,
       -6,    -6,    -7,    -7,    -7,    -8,    -8,    -9,
       -9,   -10,   -10,   -10,   -11,   -11,   -12,   -12,
      -13,   -13,   -14,   -14,   -14,   -15,   -15,   -16,
      -16,   -17,   -17,   -18,   -18,   -19,   -20,   -20,
      -21,   -21,   -22,   -22,   -23,   -23,   -24,   -25,
      -25,   -26,   -26,   -27,   -28,   -28,   -29,   -30,
      -30,   -31,   -32,   -33,   -33,   -34,   -35,   -36,
      -36,   -37,   -38,   -39,   -40,   -40,   -41,   -42,
      -43,   -44,   -45,   -46,   -47,   -48,   -49,   -50,
      -51,   -52,   -54,   -55,   -56,   -57,   -59,   -60,
      -61,   -63,   -64,   -66,   -67,   -69,   -71,   -72,
      -74,   -76,   -78,   -80,   -83,   -85,   -87,   -90,
      -93,   -96,   -99,  -102,  -106,  -110,  -115,  -120,
     -126,  -133,  -140,  -150,  -163,  -180,  -210,  -904
};
static s16 __MIX_DPL2_front[128] = {
       0,    0,    0,    0,    0,    0,    0,    0,
       0,    0,    0,    0,    0,   -1,   -1,   -1,
      -1,   -1,   -2,   -2,   -2,   -2,   -3,   -3,
      -3,   -4,   -4,   -4,   -5,   -5,   -6,   -6,
      -6,   -7,   -7,   -8,   -8,   -9,   -9,  -10,
     -11,  -11,  -12,  -12,  -13,  -14,  -14,  -15,
     -16,  -17,  -17,  -18,  -19,  -20,  -21,  -21,
     -22,  -23,  -24,  -25,  -26,  -27,  -28,  -29,
     -30,  -31,  -32,  -34,  -35,  -36,  -37,  -38,
     -40,  -41,  -42,  -44,  -45,  -47,  -48,  -50,
     -52,  -53,  -55,  -57,  -58,  -60,  -62,  -64,
     -66,  -68,  -70,  -73,  -75,  -77,  -80,  -82,
     -85,  -88,  -90,  -93,  -96, -100, -103, -106,
    -110, -114, -118, -122, -126, -131, -136, -141,
    -146, -152, -159, -166, -173, -181, -190, -201,
    -212, -225, -241, -261, -286, -321, -381, -960
};
static s16 __MIX_DPL2_rear[128] = {
     -61,  -61,  -60,  -59,  -59,  -58,  -58,  -57,
     -56,  -56,  -55,  -55,  -54,  -53,  -53,  -52,
     -52,  -51,  -50,  -50,  -49,  -49,  -48,  -48,
     -47,  -47,  -46,  -46,  -45,  -45,  -44,  -44,
     -43,  -43,  -42,  -42,  -41,  -41,  -40,  -40,
     -39,  -39,  -38,  -38,  -38,  -37,  -37,  -36,
     -36,  -35,  -35,  -35,  -34,  -34,  -33,  -33,
     -32,  -32,  -32,  -31,  -31,  -31,  -30,  -30,
     -29,  -29,  -29,  -28,  -28,  -28,  -27,  -27,
     -27,  -26,  -26,  -26,  -25,  -25,  -25,  -24,
     -24,  -24,  -23,  -23,  -23,  -22,  -22,  -22,
     -21,  -21,  -21,  -20,  -20,  -20,  -20,  -19,
     -19,  -19,  -18,  -18,  -18,  -18,  -17,  -17,
     -17,  -17,  -16,  -16,  -16,  -16,  -15,  -15,
     -15,  -15,  -14,  -14,  -14,  -14,  -13,  -13,
     -13,  -13,  -13,  -12,  -12,  -12,  -12,  -11
};

static MIXChannel __s_MIXChannel[MIX_MAX_VOICES];
static MIXRmtChannel __s_MIXRmtChannel[MIX_MAX_VOICES];

MIXRmtChannel* __MIXRmtChannel;

static u32 __MIXSoundMode;
static int __MIXMaxVoices;
static BOOL __MIXInitialized;
static MIXChannel* __MIXChannel;

static void __MIXSetPan(MIXChannel* channel);

static int __MIXClampPan(int pan) {
    if (pan < 0) {
        return 0;
    }
    if (pan > 0x7F) {
        return 0x7F;
    }
    return pan;
}

/*
 * __MIXSetPan
 *
 * Looks up the front/back and left/right gains of a voice's pan and surround pan: the plain pan table, or the two
 * DPL2 tables (the DPL2 rear pair goes in l1/r1).
 */
static void __MIXSetPan(MIXChannel* channel) {
    int pan, span;
    int ipan, ispan;

    pan = channel->pan;
    ipan = 127 - pan;
    span = channel->span;
    ispan = 127 - span;

    if (__MIXSoundMode == MIX_SOUND_MODE_DPL2) {
        channel->l = __MIX_DPL2_front[pan];
        channel->r = __MIX_DPL2_front[ipan];
        channel->f = __MIX_DPL2_front[ispan];
        channel->b = __MIX_DPL2_front[span];
        channel->l1 = __MIX_DPL2_rear[ipan];
        channel->r1 = __MIX_DPL2_rear[pan];
    } else {
        channel->l = __MIXPanTable[pan];
        channel->r = __MIXPanTable[ipan];
        channel->f = __MIXPanTable[ispan];
        channel->b = __MIXPanTable[span];
        channel->l1 = 0;
        channel->r1 = 0;
    }
} // 0x802334C0

u16 __MIXGetVolume(int db_) {
    int db = db_;
    if (db <= -0x388) {
        return 0;
    }
    if (db >= 0x3C) {
        return 0xFF64;
    }
    return __MIXVolumeTable[db + 0x388];
} // 0x80233580

static void __MIXResetChannel(MIXChannel* channel) {
    channel->mode = 0x50000000;
    channel->input = 0;
    channel->auxA = -0x3C0;
    channel->auxB = -0x3C0;
    channel->auxC = -0x3C0;
    channel->fader = 0;
    channel->pan = 0x40;
    channel->span = 0x7F;

    channel->v = channel->vL = channel->vR = channel->vS = channel->vAL = channel->vAR = channel->vAS = channel->vBL =
        channel->vBR = channel->vBS = channel->vCL = channel->vCR = channel->vCS = 0;

    __MIXSetPan(channel);
}

void MIXInit(void) {
    u32 i;

    if (AXIsInit() && !__MIXInitialized) {
        __MIXMaxVoices = AXGetMaxVoices();
        __MIXChannel = __s_MIXChannel;
        __MIXRmtChannel = __s_MIXRmtChannel;

        for (i = 0; i < __MIXMaxVoices; i++) {
            __MIXChannel[i].axvpb = 0;
            __MIXResetChannel(&__MIXChannel[i]);
            __MIXResetRmtChannel(i);
        }

        __MIXSoundMode = MIX_SOUND_MODE_STEREO;
        __MIXInitialized = TRUE;
    }
} // 0x802335C0

void MIXQuit(void) {
    __MIXChannel = 0;
    __MIXRmtChannel = 0;
    __MIXInitialized = FALSE;
} // 0x802336E0

void MIXSetSoundMode(u32 mode) {
    __MIXSoundMode = mode;
} // 0x80233700

void MIXInitChannel(AXVPB* axvpb, u32 mode, int input, int auxA, int auxB, int auxC, int pan, int span, int fader) {
    BOOL old;
    MIXChannel* c;
    u32 mixerCtrl;
    u16* p;

    c = &__MIXChannel[axvpb->index];

    c->axvpb = axvpb;
    c->mode = mode & MIX_MODE_MASK;
    c->input = input;
    c->auxA = auxA;
    c->auxB = auxB;
    c->auxC = auxC;
    c->pan = pan;
    c->span = span;
    c->fader = fader;

    __MIXSetPan(c);

    if (c->mode & MIX_MODE_MUTE) {
        c->v = 0;
    } else {
        c->v = __MIXGetVolume(input);
    }

    mixerCtrl = 0;

    switch (__MIXSoundMode) {
    case MIX_SOUND_MODE_MONO:
        c->vL = __MIXGetVolume(c->fader + c->f);
        c->vR = __MIXGetVolume(c->fader + c->f);
        c->vS = __MIXGetVolume(c->fader + c->b - 30);

        if (c->mode & MIX_MODE_AUXA_PRE) {
            c->vAL = __MIXGetVolume(c->auxA + c->f);
            c->vAR = __MIXGetVolume(c->auxA + c->f);
            c->vAS = __MIXGetVolume(c->auxA + c->b - 30);
        } else {
            c->vAL = __MIXGetVolume(c->fader + c->auxA + c->f);
            c->vAR = __MIXGetVolume(c->fader + c->auxA + c->f);
            c->vAS = __MIXGetVolume(c->fader + c->auxA + c->b - 30);
        }

        if (c->mode & MIX_MODE_AUXB_PRE) {
            c->vBL = __MIXGetVolume(c->auxB + c->f);
            c->vBR = __MIXGetVolume(c->auxB + c->f);
            c->vBS = __MIXGetVolume(c->auxB + c->b - 30);
        } else {
            c->vBL = __MIXGetVolume(c->fader + c->auxB + c->f);
            c->vBR = __MIXGetVolume(c->fader + c->auxB + c->f);
            c->vBS = __MIXGetVolume(c->fader + c->auxB + c->b - 30);
        }

        if (c->mode & MIX_MODE_AUXC_PRE) {
            c->vCL = __MIXGetVolume(c->auxC + c->f);
            c->vCR = __MIXGetVolume(c->auxC + c->f);
            c->vCS = __MIXGetVolume(c->auxC + c->b - 30);
        } else {
            c->vCL = __MIXGetVolume(c->fader + c->auxC + c->f);
            c->vCR = __MIXGetVolume(c->fader + c->auxC + c->f);
            c->vCS = __MIXGetVolume(c->fader + c->auxC + c->b - 30);
        }
        break;
    case MIX_SOUND_MODE_STEREO:
    case MIX_SOUND_MODE_SURROUND:
        c->vL = __MIXGetVolume(c->fader + c->l + c->f);
        c->vR = __MIXGetVolume(c->fader + c->r + c->f);
        c->vS = __MIXGetVolume(c->fader + c->b - 30);

        if (c->mode & MIX_MODE_AUXA_PRE) {
            c->vAL = __MIXGetVolume(c->auxA + c->l + c->f);
            c->vAR = __MIXGetVolume(c->auxA + c->r + c->f);
            c->vAS = __MIXGetVolume(c->auxA + c->b - 30);
        } else {
            c->vAL = __MIXGetVolume(c->fader + c->auxA + c->l + c->f);
            c->vAR = __MIXGetVolume(c->fader + c->auxA + c->r + c->f);
            c->vAS = __MIXGetVolume(c->fader + c->auxA + c->b - 30);
        }

        if (c->mode & MIX_MODE_AUXB_PRE) {
            c->vBL = __MIXGetVolume(c->auxB + c->l + c->f);
            c->vBR = __MIXGetVolume(c->auxB + c->r + c->f);
            c->vBS = __MIXGetVolume(c->auxB + c->b - 30);
        } else {
            c->vBL = __MIXGetVolume(c->fader + c->auxB + c->l + c->f);
            c->vBR = __MIXGetVolume(c->fader + c->auxB + c->r + c->f);
            c->vBS = __MIXGetVolume(c->fader + c->auxB + c->b - 30);
        }

        if (c->mode & MIX_MODE_AUXC_PRE) {
            c->vCL = __MIXGetVolume(c->auxC + c->l + c->f);
            c->vCR = __MIXGetVolume(c->auxC + c->r + c->f);
            c->vCS = __MIXGetVolume(c->auxC + c->b - 30);
        } else {
            c->vCL = __MIXGetVolume(c->fader + c->auxC + c->l + c->f);
            c->vCR = __MIXGetVolume(c->fader + c->auxC + c->r + c->f);
            c->vCS = __MIXGetVolume(c->fader + c->auxC + c->b - 30);
        }
        break;
    case MIX_SOUND_MODE_DPL2:
        c->vL = __MIXGetVolume(c->fader + c->l + c->f);
        c->vR = __MIXGetVolume(c->fader + c->r + c->f);
        c->vS = __MIXGetVolume(c->fader + c->l1 + c->b);
        c->vCL = __MIXGetVolume(c->fader + c->r1 + c->b);

        if (c->mode & MIX_MODE_AUXA_PRE) {
            c->vAL = __MIXGetVolume(c->auxA + c->l + c->f);
            c->vAR = __MIXGetVolume(c->auxA + c->r + c->f);
            c->vAS = __MIXGetVolume(c->auxA + c->l1 + c->b);
            c->vCR = __MIXGetVolume(c->auxA + c->r1 + c->b);
        } else {
            c->vAL = __MIXGetVolume(c->fader + c->auxA + c->l + c->f);
            c->vAR = __MIXGetVolume(c->fader + c->auxA + c->r + c->f);
            c->vAS = __MIXGetVolume(c->fader + c->auxA + c->l1 + c->b);
            c->vCR = __MIXGetVolume(c->fader + c->auxA + c->r1 + c->b);
        }

        if (c->mode & MIX_MODE_AUXB_PRE) {
            c->vBL = __MIXGetVolume(c->auxB + c->l + c->f);
            c->vBR = __MIXGetVolume(c->auxB + c->r + c->f);
            c->vBS = __MIXGetVolume(c->auxB + c->l1 + c->b);
            c->vCS = __MIXGetVolume(c->auxB + c->r1 + c->b);
        } else {
            c->vBL = __MIXGetVolume(c->fader + c->auxB + c->l + c->f);
            c->vBR = __MIXGetVolume(c->fader + c->auxB + c->r + c->f);
            c->vBS = __MIXGetVolume(c->fader + c->auxB + c->l1 + c->b);
            c->vCS = __MIXGetVolume(c->fader + c->auxB + c->r1 + c->b);
        }

        mixerCtrl |= MIX_CTRL_DPL2;
        break;
    }

    __MIXResetRmtChannel(axvpb->index);

    old = OSDisableInterrupts();
    axvpb->pb.ve.currentVolume = c->v;
    axvpb->pb.ve.currentDelta = 0;

    p = (u16*)&axvpb->pb.mix;

    if ((*p++ = c->vL))
        mixerCtrl |= MIX_CTRL_L;
    *p++ = 0;

    if ((*p++ = c->vR))
        mixerCtrl |= MIX_CTRL_R;
    *p++ = 0;

    if ((*p++ = c->vAL))
        mixerCtrl |= MIX_CTRL_AL;
    *p++ = 0;

    if ((*p++ = c->vAR))
        mixerCtrl |= MIX_CTRL_AR;
    *p++ = 0;

    if ((*p++ = c->vBL))
        mixerCtrl |= MIX_CTRL_BL;
    *p++ = 0;

    if ((*p++ = c->vBR))
        mixerCtrl |= MIX_CTRL_BR;
    *p++ = 0;

    if ((*p++ = c->vCL))
        mixerCtrl |= MIX_CTRL_CL;
    *p++ = 0;

    if ((*p++ = c->vCR))
        mixerCtrl |= MIX_CTRL_CR;
    *p++ = 0;

    if ((*p++ = c->vS))
        mixerCtrl |= MIX_CTRL_S;
    *p++ = 0;

    if ((*p++ = c->vAS))
        mixerCtrl |= MIX_CTRL_AS;
    *p++ = 0;

    if ((*p++ = c->vBS))
        mixerCtrl |= MIX_CTRL_BS;
    *p++ = 0;

    if ((*p++ = c->vCS))
        mixerCtrl |= MIX_CTRL_CS;
    *p++ = 0;

    axvpb->pb.mixerCtrl = mixerCtrl;
    axvpb->sync |= AX_PBSYNC_MIXER_CTRL | AX_PBSYNC_MIX | AX_PBSYNC_VE;
    memset(&axvpb->pb.rmtMix, 0, sizeof(axvpb->pb.rmtMix));
    axvpb->pb.rmtMixerCtrl = 0;
    axvpb->sync |= AX_PBSYNC_RMT_MIXER_CTRL | AX_PBSYNC_RMTMIX;
    OSRestoreInterrupts(old);
} // 0x80233710

void MIXReleaseChannel(AXVPB* axvpb) {
    __MIXChannel[axvpb->index].axvpb = 0;
} // 0x80234DE0

void MIXResetControls(AXVPB* p) {
    __MIXResetChannel(&__MIXChannel[p->index]);
    __MIXResetRmtChannel(p->index);
} // 0x80234E00

void MIXSetInput(AXVPB* p, int dB) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->input = dB;
    channel->mode |= MIX_FLAG_INPUT_DIRTY;
} // 0x80234EB0

void MIXSetAuxA(AXVPB* p, int dB) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->auxA = dB;
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80234EE0

void MIXSetAuxB(AXVPB* p, int dB) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->auxB = dB;
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80234F10

void MIXSetAuxC(AXVPB* p, int dB) {
    MIXChannel* channel;
    if (__MIXSoundMode == MIX_SOUND_MODE_DPL2)
        return;

    channel = &__MIXChannel[p->index];
    channel->auxC = dB;
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80234F40

void MIXSetPan(AXVPB* p, int pan) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->pan = __MIXClampPan(pan);
    __MIXSetPan(channel);
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80234F70

void MIXSetSPan(AXVPB* p, int span) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->span = __MIXClampPan(span);
    __MIXSetPan(channel);
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80234FE0

void MIXSetFader(AXVPB* p, int dB) {
    MIXChannel* channel;

    channel = &__MIXChannel[p->index];
    channel->fader = dB;
    channel->mode |= MIX_FLAG_MIX_DIRTY;
} // 0x80235050

void MIXUpdateSettings(void) {
    u32 i;
    int setNewMixLevel;
    int setNewInputLevel;
    MIXChannel* c;
    AXVPB* axvpb;
    u32 mixerCtrl;
    u16* p;

    if (__MIXInitialized) {
        for (i = 0; i < __MIXMaxVoices; i++) {
            setNewInputLevel = 0;
            setNewMixLevel = 0;

            c = &__MIXChannel[i];
            axvpb = c->axvpb;

            if (axvpb) {
                mixerCtrl = 0;

                if (c->mode & MIX_FLAG_INPUT_APPLY) {
                    c->v = c->v1;
                    c->mode &= ~MIX_FLAG_INPUT_APPLY;
                    setNewInputLevel = TRUE;
                }

                if (c->mode & MIX_FLAG_INPUT_DIRTY) {
                    if (c->mode & MIX_MODE_MUTE) {
                        c->v1 = 0;
                    } else {
                        c->v1 = __MIXGetVolume(c->input);
                    }

                    c->mode &= ~MIX_FLAG_INPUT_DIRTY;
                    c->mode |= MIX_FLAG_INPUT_APPLY;
                    setNewInputLevel = TRUE;
                }

                if (c->mode & MIX_FLAG_MIX_APPLY) {
                    c->vL = c->vL1;
                    c->vR = c->vR1;
                    c->vS = c->vS1;
                    c->vAL = c->vAL1;
                    c->vAR = c->vAR1;
                    c->vAS = c->vAS1;
                    c->vBL = c->vBL1;
                    c->vBR = c->vBR1;
                    c->vBS = c->vBS1;
                    c->vCL = c->vCL1;
                    c->vCR = c->vCR1;
                    c->vCS = c->vCS1;

                    c->mode &= ~MIX_FLAG_MIX_APPLY;
                    setNewMixLevel = TRUE;
                }

                if (c->mode & MIX_FLAG_MIX_DIRTY) {
                    switch (__MIXSoundMode) {
                    case MIX_SOUND_MODE_MONO:
                        c->vL1 = __MIXGetVolume(c->fader + c->f);
                        c->vR1 = __MIXGetVolume(c->fader + c->f);
                        c->vS1 = __MIXGetVolume(c->fader + c->b - 30);

                        if (c->mode & MIX_MODE_AUXA_PRE) {
                            c->vAL1 = __MIXGetVolume(c->auxA + c->f);
                            c->vAR1 = __MIXGetVolume(c->auxA + c->f);
                            c->vAS1 = __MIXGetVolume(c->auxA + c->b - 30);
                        } else {
                            c->vAL1 = __MIXGetVolume(c->fader + c->auxA + c->f);
                            c->vAR1 = __MIXGetVolume(c->fader + c->auxA + c->f);
                            c->vAS1 = __MIXGetVolume(c->fader + c->auxA + c->b - 30);
                        }

                        if (c->mode & MIX_MODE_AUXB_PRE) {
                            c->vBL1 = __MIXGetVolume(c->auxB + c->f);
                            c->vBR1 = __MIXGetVolume(c->auxB + c->f);
                            c->vBS1 = __MIXGetVolume(c->auxB + c->b - 30);
                        } else {
                            c->vBL1 = __MIXGetVolume(c->fader + c->auxB + c->f);
                            c->vBR1 = __MIXGetVolume(c->fader + c->auxB + c->f);
                            c->vBS1 = __MIXGetVolume(c->fader + c->auxB + c->b - 30);
                        }

                        if (c->mode & MIX_MODE_AUXC_PRE) {
                            c->vCL1 = __MIXGetVolume(c->auxC + c->f);
                            c->vCR1 = __MIXGetVolume(c->auxC + c->f);
                            c->vCS1 = __MIXGetVolume(c->auxC + c->b - 30);
                        } else {
                            c->vCL1 = __MIXGetVolume(c->fader + c->auxC + c->f);
                            c->vCR1 = __MIXGetVolume(c->fader + c->auxC + c->f);
                            c->vCS1 = __MIXGetVolume(c->fader + c->auxC + c->b - 30);
                        }
                        break;
                    case MIX_SOUND_MODE_STEREO:
                    case MIX_SOUND_MODE_SURROUND:
                        c->vL1 = __MIXGetVolume(c->fader + c->l + c->f);
                        c->vR1 = __MIXGetVolume(c->fader + c->r + c->f);
                        c->vS1 = __MIXGetVolume(c->fader + c->b - 30);

                        if (c->mode & MIX_MODE_AUXA_PRE) {
                            c->vAL1 = __MIXGetVolume(c->auxA + c->l + c->f);
                            c->vAR1 = __MIXGetVolume(c->auxA + c->r + c->f);
                            c->vAS1 = __MIXGetVolume(c->auxA + c->b - 30);
                        } else {
                            c->vAL1 = __MIXGetVolume(c->fader + c->auxA + c->l + c->f);
                            c->vAR1 = __MIXGetVolume(c->fader + c->auxA + c->r + c->f);
                            c->vAS1 = __MIXGetVolume(c->fader + c->auxA + c->b - 30);
                        }

                        if (c->mode & MIX_MODE_AUXB_PRE) {
                            c->vBL1 = __MIXGetVolume(c->auxB + c->l + c->f);
                            c->vBR1 = __MIXGetVolume(c->auxB + c->r + c->f);
                            c->vBS1 = __MIXGetVolume(c->auxB + c->b - 30);
                        } else {
                            c->vBL1 = __MIXGetVolume(c->fader + c->auxB + c->l + c->f);
                            c->vBR1 = __MIXGetVolume(c->fader + c->auxB + c->r + c->f);
                            c->vBS1 = __MIXGetVolume(c->fader + c->auxB + c->b - 30);
                        }

                        if (c->mode & MIX_MODE_AUXC_PRE) {
                            c->vCL1 = __MIXGetVolume(c->auxC + c->l + c->f);
                            c->vCR1 = __MIXGetVolume(c->auxC + c->r + c->f);
                            c->vCS1 = __MIXGetVolume(c->auxC + c->b - 30);
                        } else {
                            c->vCL1 = __MIXGetVolume(c->fader + c->auxC + c->l + c->f);
                            c->vCR1 = __MIXGetVolume(c->fader + c->auxC + c->r + c->f);
                            c->vCS1 = __MIXGetVolume(c->fader + c->auxC + c->b - 30);
                        }
                        break;
                    case MIX_SOUND_MODE_DPL2:
                        c->vL1 = __MIXGetVolume(c->fader + c->l + c->f);
                        c->vR1 = __MIXGetVolume(c->fader + c->r + c->f);
                        c->vS1 = __MIXGetVolume(c->fader + c->l1 + c->b);
                        c->vCL1 = __MIXGetVolume(c->fader + c->r1 + c->b);

                        if (c->mode & MIX_MODE_AUXA_PRE) {
                            c->vAL1 = __MIXGetVolume(c->auxA + c->l + c->f);
                            c->vAR1 = __MIXGetVolume(c->auxA + c->r + c->f);
                            c->vAS1 = __MIXGetVolume(c->auxA + c->l1 + c->b);
                            c->vCR1 = __MIXGetVolume(c->auxA + c->r1 + c->b);
                        } else {
                            c->vAL1 = __MIXGetVolume(c->fader + c->auxA + c->l + c->f);
                            c->vAR1 = __MIXGetVolume(c->fader + c->auxA + c->r + c->f);
                            c->vAS1 = __MIXGetVolume(c->fader + c->auxA + c->l1 + c->b);
                            c->vCR1 = __MIXGetVolume(c->fader + c->auxA + c->r1 + c->b);
                        }

                        if (c->mode & MIX_MODE_AUXB_PRE) {
                            c->vBL1 = __MIXGetVolume(c->auxB + c->l + c->f);
                            c->vBR1 = __MIXGetVolume(c->auxB + c->r + c->f);
                            c->vBS1 = __MIXGetVolume(c->auxB + c->l1 + c->b);
                            c->vCS1 = __MIXGetVolume(c->auxB + c->r1 + c->b);
                        } else {
                            c->vBL1 = __MIXGetVolume(c->fader + c->auxB + c->l + c->f);
                            c->vBR1 = __MIXGetVolume(c->fader + c->auxB + c->r + c->f);
                            c->vBS1 = __MIXGetVolume(c->fader + c->auxB + c->l1 + c->b);
                            c->vCS1 = __MIXGetVolume(c->fader + c->auxB + c->r1 + c->b);
                        }

                        mixerCtrl |= MIX_CTRL_DPL2;
                        break;
                    }

                    c->mode &= ~MIX_FLAG_MIX_DIRTY;
                    c->mode |= MIX_FLAG_MIX_APPLY;
                    setNewMixLevel = TRUE;
                }

                if (setNewInputLevel) {
                    axvpb->pb.ve.currentVolume = c->v;
                    axvpb->pb.ve.currentDelta = (s16)((c->v1 - c->v) / AX_SAMPLES_PER_FRAME);
                    axvpb->sync |= AX_PBSYNC_VE;
                }

                if (setNewMixLevel) {
                    p = (u16*)&axvpb->pb.mix;

                    if ((*p++ = c->vL))
                        mixerCtrl |= MIX_CTRL_L;

                    if ((*p++ = (u16)((c->vL1 - c->vL) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_L;

                    if ((*p++ = c->vR))
                        mixerCtrl |= MIX_CTRL_R;

                    if ((*p++ = (u16)((c->vR1 - c->vR) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_L;

                    if ((*p++ = c->vAL))
                        mixerCtrl |= MIX_CTRL_AL;

                    if ((*p++ = (u16)((c->vAL1 - c->vAL) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_A;

                    if ((*p++ = c->vAR))
                        mixerCtrl |= MIX_CTRL_AR;

                    if ((*p++ = (u16)((c->vAR1 - c->vAR) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_A;

                    if ((*p++ = c->vBL))
                        mixerCtrl |= MIX_CTRL_BL;

                    if ((*p++ = (u16)((c->vBL1 - c->vBL) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_B;

                    if ((*p++ = c->vBR))
                        mixerCtrl |= MIX_CTRL_BR;

                    if ((*p++ = (u16)((c->vBR1 - c->vBR) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_B;

                    if ((*p++ = c->vCL))
                        mixerCtrl |= MIX_CTRL_CL;

                    if ((*p++ = (u16)((c->vCL1 - c->vCL) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_C;

                    if ((*p++ = c->vCR))
                        mixerCtrl |= MIX_CTRL_CR;

                    if ((*p++ = (u16)((c->vCR1 - c->vCR) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_C;

                    if ((*p++ = c->vS))
                        mixerCtrl |= MIX_CTRL_S;

                    if ((*p++ = (u16)((c->vS1 - c->vS) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_S;

                    if ((*p++ = c->vAS))
                        mixerCtrl |= MIX_CTRL_AS;

                    if ((*p++ = (u16)((c->vAS1 - c->vAS) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_AS;

                    if ((*p++ = c->vBS))
                        mixerCtrl |= MIX_CTRL_BS;

                    if ((*p++ = (u16)((c->vBS1 - c->vBS) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_BS;

                    if ((*p++ = c->vCS))
                        mixerCtrl |= MIX_CTRL_CS;

                    if ((*p++ = (u16)((c->vCS1 - c->vCS) / AX_SAMPLES_PER_FRAME)))
                        mixerCtrl |= MIX_CTRL_DELTA_CS;

                    axvpb->pb.mixerCtrl = mixerCtrl;
                    axvpb->sync |= AX_PBSYNC_MIXER_CTRL | AX_PBSYNC_MIX;
                }

                __MIXUpdateRmtChannel(i, axvpb);
            }
        }
    }
} // 0x80235080
