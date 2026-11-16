#include <revolution/gx.h>
#include <revolution/os.h>

#include <revolution/gx/__gx.h>

/* Bits of a stage's alpha environment word that GXSetTevOp keeps: the register id and the swap-table selects. */
#define TEV_ALPHA_ENV_SWAPMODE_MASK (TEV_ALPHA_ENV_RID_MASK | TEV_ALPHA_ENV_SWAP_MASK | TEV_ALPHA_ENV_MODE_MASK)

static struct {
    u32 rid : 8;
    u32 dest : 2;
    u32 shift : 2;
    u32 clamp : 1;
    u32 sub : 1;
    u32 bias : 2;
    u32 sela : 4;
    u32 selb : 4;
    u32 selc : 4;
    u32 seld : 4;
} TEVCOpTableST0[5] = {
    {192, 0, 0, 1, 0, 0, 15, 8, 10, 15},   // modulate
    {192, 0, 0, 1, 0, 0, 10, 8, 9, 15},    // decal
    {192, 0, 0, 1, 0, 0, 10, 12, 8, 15},   // blend
    {192, 0, 0, 1, 0, 0, 15, 15, 15, 8},   // replace
    {192, 0, 0, 1, 0, 0, 15, 15, 15, 10},  // passclr
};

static struct {
    u32 rid : 8;
    u32 dest : 2;
    u32 shift : 2;
    u32 clamp : 1;
    u32 sub : 1;
    u32 bias : 2;
    u32 sela : 4;
    u32 selb : 4;
    u32 selc : 4;
    u32 seld : 4;
} TEVCOpTableST1[5] = {
    {192, 0, 0, 1, 0, 0, 15, 8, 0, 15},   // modulate
    {192, 0, 0, 1, 0, 0, 0, 8, 9, 15},    // decal
    {192, 0, 0, 1, 0, 0, 0, 12, 8, 15},   // blend
    {192, 0, 0, 1, 0, 0, 15, 15, 15, 8},  // replace
    {192, 0, 0, 1, 0, 0, 15, 15, 15, 0},  // passclr
};

static struct {
    u32 rid : 8;
    u32 dest : 2;
    u32 shift : 2;
    u32 clamp : 1;
    u32 sub : 1;
    u32 bias : 2;
    u32 sela : 3;
    u32 selb : 3;
    u32 selc : 3;
    u32 seld : 3;
    u32 swap : 2;
    u32 mode : 2;
} TEVAOpTableST0[5] = {
    {193, 0, 0, 1, 0, 0, 7, 4, 5, 7, 0, 0},  // modulate
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 5, 0, 0},  // decal
    {193, 0, 0, 1, 0, 0, 7, 4, 5, 7, 0, 0},  // blend
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 4, 0, 0},  // replace
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 5, 0, 0},  // passclr
};

static struct {
    u32 rid : 8;
    u32 dest : 2;
    u32 shift : 2;
    u32 clamp : 1;
    u32 sub : 1;
    u32 bias : 2;
    u32 sela : 3;
    u32 selb : 3;
    u32 selc : 3;
    u32 seld : 3;
    u32 swap : 2;
    u32 mode : 2;
} TEVAOpTableST1[5] = {
    {193, 0, 0, 1, 0, 0, 7, 4, 0, 7, 0, 0},  // modulate
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 0, 0, 0},  // decal
    {193, 0, 0, 1, 0, 0, 7, 4, 0, 7, 0, 0},  // blend
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 4, 0, 0},  // replace
    {193, 0, 0, 1, 0, 0, 7, 7, 7, 0, 0, 0},  // passclr
};

// Sets a TEV stage to one of the predefined modes (modulate, decal, ...).
// 0x8023D4F0
void GXSetTevOp(GXTevStageID id, GXTevMode mode) {
    u32* ctmp;
    u32* atmp;
    u32 tevReg;

    CHECK_GXBEGIN("GXSetTevOp");
    ASSERTMSG(id < GX_MAX_TEVSTAGE, "GXSetTevColor*: Invalid Tev Stage Index");
    ASSERTMSG(mode <= GX_PASSCLR, "GXSetTevOp: Invalid Tev Mode");

    if (id == GX_TEVSTAGE0) {
        ctmp = (u32*)&TEVCOpTableST0[mode];
        atmp = (u32*)&TEVAOpTableST0[mode];
    } else {
        ctmp = (u32*)&TEVCOpTableST1[mode];
        atmp = (u32*)&TEVAOpTableST1[mode];
    }

    tevReg = __GXData->tevc[id];
    tevReg = (*ctmp & ~TEV_COLOR_ENV_RID_MASK) | (tevReg & TEV_COLOR_ENV_RID_MASK);
    GX_WRITE_RAS_REG(tevReg);
    __GXData->tevc[id] = tevReg;

    tevReg = __GXData->teva[id];
    tevReg = (*atmp & ~TEV_ALPHA_ENV_SWAPMODE_MASK) | (tevReg & TEV_ALPHA_ENV_SWAPMODE_MASK);
    GX_WRITE_RAS_REG(tevReg);
    __GXData->teva[id] = tevReg;

    __GXData->bpSentNot = 0;
}

// Sets the four colour inputs of a TEV stage.
// 0x8023D590
void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {
    u32 tevReg;

    CHECK_GXBEGIN("GXSetTevColorIn");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevColor*: Invalid Tev Stage Index");
    ASSERTMSG(a <= GX_CC_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(b <= GX_CC_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(c <= GX_CC_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(d <= GX_CC_ZERO, "GXSetTev*In: A/B/C/D argument out of range");

    tevReg = __GXData->tevc[stage];
    SC_TEV_COLOR_ENV_SET_SELA(591, tevReg, a);
    SC_TEV_COLOR_ENV_SET_SELB(592, tevReg, b);
    SC_TEV_COLOR_ENV_SET_SELC(593, tevReg, c);
    SC_TEV_COLOR_ENV_SET_SELD(594, tevReg, d);

    GX_WRITE_RAS_REG(tevReg);
    __GXData->tevc[stage] = tevReg;
    __GXData->bpSentNot = 0;
}

// Sets the four alpha inputs of a TEV stage.
// 0x8023D5D0
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {
    u32 tevReg;

    CHECK_GXBEGIN("GXSetTevAlphaIn");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevAlpha*: Invalid Tev Stage Index");
    ASSERTMSG(a <= GX_CA_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(b <= GX_CA_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(c <= GX_CA_ZERO, "GXSetTev*In: A/B/C/D argument out of range");
    ASSERTMSG(d <= GX_CA_ZERO, "GXSetTev*In: A/B/C/D argument out of range");

    tevReg = __GXData->teva[stage];
    SC_TEV_ALPHA_ENV_SET_SELA(627, tevReg, a);
    SC_TEV_ALPHA_ENV_SET_SELB(628, tevReg, b);
    SC_TEV_ALPHA_ENV_SET_SELC(629, tevReg, c);
    SC_TEV_ALPHA_ENV_SET_SELD(630, tevReg, d);

    GX_WRITE_RAS_REG(tevReg);
    __GXData->teva[stage] = tevReg;
    __GXData->bpSentNot = 0;
}

// Sets the colour operation, bias, scale, clamp and output of a TEV stage.
// 0x8023D610
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    u32 tevReg;

    CHECK_GXBEGIN("GXSetTevColorOp");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevColor*: Invalid Tev Stage Index");

    tevReg = __GXData->tevc[stage];
    SC_TEV_COLOR_ENV_SET_SUB(668, tevReg, op & 1);
    if (op <= 1) {
        SC_TEV_COLOR_ENV_SET_SHIFT(670, tevReg, scale);
        SC_TEV_COLOR_ENV_SET_BIAS(671, tevReg, bias);
    } else {
        SC_TEV_COLOR_ENV_SET_SHIFT(673, tevReg, (op >> 1) & 3);
        SC_TEV_COLOR_ENV_SET_BIAS(674, tevReg, 3);
    }
    SC_TEV_COLOR_ENV_SET_CLAMP(677, tevReg, clamp);
    SC_TEV_COLOR_ENV_SET_DEST(678, tevReg, out_reg);

    GX_WRITE_RAS_REG(tevReg);
    __GXData->tevc[stage] = tevReg;
    __GXData->bpSentNot = 0;
}

// Sets the alpha operation, bias, scale, clamp and output of a TEV stage.
// 0x8023D670
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out_reg) {
    u32 tevReg;

    CHECK_GXBEGIN("GXSetTevAlphaOp");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevAlpha*: Invalid Tev Stage Index");

    tevReg = __GXData->teva[stage];
    SC_TEV_ALPHA_ENV_SET_SUB(713, tevReg, op & 1);
    if (op <= 1) {
        SC_TEV_ALPHA_ENV_SET_SHIFT(715, tevReg, scale);
        SC_TEV_ALPHA_ENV_SET_BIAS(716, tevReg, bias);
    } else {
        SC_TEV_ALPHA_ENV_SET_SHIFT(718, tevReg, (op >> 1) & 3);
        SC_TEV_ALPHA_ENV_SET_BIAS(719, tevReg, 3);
    }
    SC_TEV_ALPHA_ENV_SET_CLAMP(722, tevReg, clamp);
    SC_TEV_ALPHA_ENV_SET_DEST(723, tevReg, out_reg);

    GX_WRITE_RAS_REG(tevReg);
    __GXData->teva[stage] = tevReg;
    __GXData->bpSentNot = 0;
}

// Sets a TEV colour register from an 8-bit colour.
// 0x8023D6D0
void GXSetTevColor(GXTevRegID id, GXColor color) {
    u32 rgba;
    u32 regRA;
    u32 regBG;

    CHECK_GXBEGIN("GXSetTevColor");
    rgba = *(u32*)&color;

    regRA = (0xE0 + id * 2) << 24;
    SC_TEV_REGISTERL_SET_R8(750, regRA, rgba >> 24);
    SC_TEV_REGISTERL_SET_A8(751, regRA, rgba & 0xFF);

    regBG = (0xE1 + id * 2) << 24;
    SC_TEV_REGISTERH_SET_B8(754, regBG, (rgba >> 8) & 0xFF);
    SC_TEV_REGISTERH_SET_G8(755, regBG, (rgba >> 16) & 0xFF);

    GX_WRITE_RAS_REG(regRA);
    GX_WRITE_RAS_REG(regBG);
    GX_WRITE_RAS_REG(regBG);
    GX_WRITE_RAS_REG(regBG);

    __GXData->bpSentNot = 0;
}

// Sets a TEV colour register from a signed 10-bit colour.
// 0x8023D730
void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {
    u32 sRG;
    u32 sBA;
    u32 regRA;
    u32 regBG;

    ASSERTMSG(color.r >= -1024 && color.r < 1024, "GXSetTevColorS10: Color not in range -1024 to +1023");
    ASSERTMSG(color.g >= -1024 && color.g < 1024, "GXSetTevColorS10: Color not in range -1024 to +1023");
    ASSERTMSG(color.b >= -1024 && color.b < 1024, "GXSetTevColorS10: Color not in range -1024 to +1023");
    ASSERTMSG(color.a >= -1024 && color.a < 1024, "GXSetTevColorS10: Color not in range -1024 to +1023");

    CHECK_GXBEGIN("GXSetTevColorS10");
    sRG = *(u32*)&color;
    sBA = *((u32*)&color + 1);

    regRA = (0xE0 + id * 2) << 24;
    SC_TEV_REGISTERL_SET_R(794, regRA, (sRG >> 16) & 0x7FF);
    SC_TEV_REGISTERL_SET_A(795, regRA, sBA & 0x7FF);

    regBG = (0xE1 + id * 2) << 24;
    SC_TEV_REGISTERH_SET_B(798, regBG, (sBA >> 16) & 0x7FF);
    SC_TEV_REGISTERH_SET_G(799, regBG, sRG & 0x7FF);

    GX_WRITE_RAS_REG(regRA);
    GX_WRITE_RAS_REG(regBG);
    GX_WRITE_RAS_REG(regBG);
    GX_WRITE_RAS_REG(regBG);

    __GXData->bpSentNot = 0;
}

// Sets a TEV constant colour register.
// 0x8023D7A0
void GXSetTevKColor(GXTevKColorID id, GXColor color) {
    u32 rgba;
    u32 regRA;
    u32 regBG;

    CHECK_GXBEGIN("GXSetTevKColor");
    rgba = *(u32*)&color;

    regRA = (0xE0 + id * 2) << 24;
    SC_TEV_KREGISTERL_SET_R(843, regRA, rgba >> 24);
    SC_TEV_KREGISTERL_SET_A(844, regRA, rgba & 0xFF);
    SC_TEV_KREGISTERL_SET_PAD1(845, regRA, 8);

    regBG = (0xE1 + id * 2) << 24;
    SC_TEV_KREGISTERH_SET_B(848, regBG, (rgba >> 8) & 0xFF);
    SC_TEV_KREGISTERH_SET_G(849, regBG, (rgba >> 16) & 0xFF);
    SC_TEV_KREGISTERH_SET_PAD1(850, regBG, 8);

    GX_WRITE_RAS_REG(regRA);
    GX_WRITE_RAS_REG(regBG);

    __GXData->bpSentNot = 0;
}

// Selects the constant colour used by a TEV stage.
// 0x8023D800
void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {
    u32* Kreg;

    CHECK_GXBEGIN("GXSetTevKColorSel");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevKColor*: Invalid Tev Stage Index");

    Kreg = &__GXData->tevKsel[stage >> 1];
    if (stage & 1) {
        SC_TEV_KSEL_SET_KCSEL1(883, *Kreg, sel);
    } else {
        SC_TEV_KSEL_SET_KCSEL0(885, *Kreg, sel);
    }

    GX_WRITE_RAS_REG(*Kreg);
    __GXData->bpSentNot = 0;
}

// Selects the constant alpha used by a TEV stage.
// 0x8023D850
void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {
    u32* Kreg;

    CHECK_GXBEGIN("GXSetTevKAlphaSel");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevKColor*: Invalid Tev Stage Index");

    Kreg = &__GXData->tevKsel[stage >> 1];
    if (stage & 1) {
        SC_TEV_KSEL_SET_KASEL1(916, *Kreg, sel);
    } else {
        SC_TEV_KSEL_SET_KASEL0(918, *Kreg, sel);
    }

    GX_WRITE_RAS_REG(*Kreg);
    __GXData->bpSentNot = 0;
}

// Selects the raster and texture swap tables of a TEV stage.
// 0x8023D8A0
void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel) {
    u32* pTevReg;

    CHECK_GXBEGIN("GXSetTevSwapMode");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevSwapMode: Invalid Tev Stage Index");

    pTevReg = &__GXData->teva[stage];
    SC_TEV_ALPHA_ENV_SET_MODE(951, *pTevReg, ras_sel);
    SC_TEV_ALPHA_ENV_SET_SWAP(952, *pTevReg, tex_sel);

    GX_WRITE_RAS_REG(*pTevReg);
    __GXData->bpSentNot = 0;
}

// Sets the channel mapping of a TEV swap table.
// 0x8023D8E0
void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red, GXTevColorChan green, GXTevColorChan blue, GXTevColorChan alpha) {
    u32* Kreg;

    CHECK_GXBEGIN("GXSetTevSwapModeTable");
    ASSERTMSG(table < GX_MAX_TEVSWAP, "GXSetTevSwapModeTable: Invalid Swap Selection Index");

    Kreg = &__GXData->tevKsel[table * 2];
    SC_TEV_KSEL_SET_XRB(987, *Kreg, red);
    SC_TEV_KSEL_SET_XGA(988, *Kreg, green);

    GX_WRITE_RAS_REG(*Kreg);

    Kreg = &__GXData->tevKsel[table * 2 + 1];
    SC_TEV_KSEL_SET_XRB(992, *Kreg, blue);
    SC_TEV_KSEL_SET_XGA(993, *Kreg, alpha);

    GX_WRITE_RAS_REG(*Kreg);
    __GXData->bpSentNot = 0;
}

// Not available on this hardware.
void GXSetTevClampMode(void) {
    ASSERTMSG(0, "GXSetTevClampMode: not available on this hardware");
}

// Sets the two alpha-test comparisons and their combining operation.
// 0x8023D960
void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    u32 reg;

    CHECK_GXBEGIN("GXSetAlphaCompare");
    reg = 0xF3000000; // BP register 0xF3 (alpha function), data still clear

    SC_TEV_ALPHAFUNC_SET_A0(1054, reg, ref0);
    SC_TEV_ALPHAFUNC_SET_A1(1055, reg, ref1);
    SC_TEV_ALPHAFUNC_SET_OP0(1056, reg, comp0);
    SC_TEV_ALPHAFUNC_SET_OP1(1057, reg, comp1);
    SC_TEV_ALPHAFUNC_SET_LOGIC(1058, reg, op);

    GX_WRITE_RAS_REG(reg);
    __GXData->bpSentNot = 0;
}

// Sets how the texture value replaces or adds to the Z value.
// 0x8023D9A0
void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias) {
    u32 zenv0;
    u32 zenv1;
    u32 type;

    CHECK_GXBEGIN("GXSetZTexture");

    zenv0 = 0;
    SC_TEV_Z_ENV_0_SET_ZOFF(1085, zenv0, bias);
    SC_TEV_Z_ENV_0_SET_RID(1086, zenv0, 0xF4);

    zenv1 = 0;
    switch (fmt) {
    case GX_TF_Z8:
        type = 0;
        break;
    case GX_TF_Z16:
        type = 1;
        break;
    case GX_TF_Z24X8:
        type = 2;
        break;
    default:
        ASSERTMSG(0, "GXSetZTexture: Invalid z-texture format");
        type = 2;
        break;
    }

    SC_TEV_Z_ENV_1_SET_TYPE(1097, zenv1, type);
    SC_TEV_Z_ENV_1_SET_OP(1098, zenv1, op);
    SC_TEV_Z_ENV_1_SET_RID(1099, zenv1, 0xF5);

    GX_WRITE_RAS_REG(zenv0);
    GX_WRITE_RAS_REG(zenv1);
    __GXData->bpSentNot = 0;
}

// Binds a TEV stage to a texture coordinate, texture map and colour channel.
// 0x8023DA30
void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {
    u32* ptref;
    u32 tmap;
    u32 tcoord;
    static int c2r[] = { 0, 1, 0, 1, 0, 1, 7, 5, 6 };

    CHECK_GXBEGIN("GXSetTevOrder");
    ASSERTMSG(stage < GX_MAX_TEVSTAGE, "GXSetTevOrder: Invalid Tev Stage Index");
    ASSERTMSG(coord < GX_MAX_TEXCOORD || coord == GX_TEXCOORD_NULL, "GXSetTevOrder: Invalid Texcoord");
    ASSERTMSG((map & ~GX_TEX_DISABLE) < GX_MAX_TEXMAP || map == GX_TEXMAP_NULL, "GXSetTevOrder: Invalid Tex Map");
    ASSERTMSG(color >= GX_COLOR0A0 && color <= GX_COLOR_NULL, "GXSetTevOrder: Invalid Color Channel ID");

    ptref = &__GXData->tref[stage / 2];
    __GXData->texmapId[stage] = map;

    tmap = map & ~GX_TEX_DISABLE;
    tmap = (tmap >= GX_MAX_TEXMAP) ? GX_TEXMAP0 : tmap;

    if (coord >= GX_MAX_TEXCOORD) {
        tcoord = GX_TEXCOORD0;
        __GXData->tevTcEnab = __GXData->tevTcEnab & ~(1 << stage);
    } else {
        tcoord = coord;
        __GXData->tevTcEnab = __GXData->tevTcEnab | (1 << stage);
    }

    if (stage & 1) {
        SC_RAS1_TREF_SET_TI1(1163, *ptref, tmap);
        SC_RAS1_TREF_SET_TC1(1164, *ptref, tcoord);
        SC_RAS1_TREF_SET_CC1(1166, *ptref, (color == GX_COLOR_NULL) ? 7 : c2r[color]);
        SC_RAS1_TREF_SET_TE1(1168, *ptref, (map != GX_TEXMAP_NULL && !(map & GX_TEX_DISABLE)));
    } else {
        SC_RAS1_TREF_SET_TI0(1171, *ptref, tmap);
        SC_RAS1_TREF_SET_TC0(1172, *ptref, tcoord);
        SC_RAS1_TREF_SET_CC0(1174, *ptref, (color == GX_COLOR_NULL) ? 7 : c2r[color]);
        SC_RAS1_TREF_SET_TE0(1176, *ptref, (map != GX_TEXMAP_NULL && !(map & GX_TEX_DISABLE)));
    }

    GX_WRITE_RAS_REG(*ptref);
    __GXData->bpSentNot = 0;
    __GXData->dirtyState |= 1;
}

// Sets the number of TEV stages in use.
// 0x8023DB90
void GXSetNumTevStages(u8 nStages) {
    CHECK_GXBEGIN("GXSetNumTevStages");

    ASSERTMSG(nStages != 0 && nStages <= 16, "GXSetNumTevStages: Exceed max number of tex stages");
    SC_GEN_MODE_SET_NTEV(1195, __GXData->genMode, nStages - 1);
    __GXData->dirtyState |= 4;
}
