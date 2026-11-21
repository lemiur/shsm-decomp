/*
 * fmod_dsp_connection_wii.cpp: the Wii mixing paths of a DSP connection (paired-single loops).
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_connection_wii.obj)
 */

extern "C" {

// Mono to stereo: paired-single loop over 4 mono frames, then the scalar remainder.
// Wall (one line): the unrolled tail reloads inbuffer[0] per statement in retail; compiled CSEs it, so the register numbers differ.
// Gormiti DWARF: ps_in0..ps_in3 (ps_volume and the output pairs are paired singles).
void FMOD_DSP_Connection_MixMonoToStereo_SIMD( register float *inbuffer, register float *outbuffer, unsigned int length, register double volumel, register double volumer ) {
    unsigned int len;
    int          count;
    register float ps_in01, ps_in23;
    register float ps_in0, ps_in1, ps_in2, ps_in3;
    register float ps_out0, ps_out1, ps_out2, ps_out3;
    register float ps_volume;
    register unsigned int outbufferend;

    len          = length;
    count        = len >> 2;
    outbufferend = (unsigned int)outbuffer + ( count << 5 );

    if ( count ) {
        asm {
            ps_merge00  ps_volume, volumel, volumer
        loop:
            psq_l       ps_in01, 0(inbuffer), 0, 0
            psq_l       ps_in23, 8(inbuffer), 0, 0
            addi        inbuffer, inbuffer, 16
            ps_merge00  ps_in0, ps_in01, ps_in01
            psq_l       ps_out0, 0(outbuffer), 0, 0
            ps_merge11  ps_in1, ps_in01, ps_in01
            psq_l       ps_out1, 8(outbuffer), 0, 0
            psq_l       ps_out2, 16(outbuffer), 0, 0
            ps_merge00  ps_in2, ps_in23, ps_in23
            ps_madd     ps_out0, ps_in0, ps_volume, ps_out0
            psq_l       ps_out3, 24(outbuffer), 0, 0
            ps_madd     ps_out1, ps_in1, ps_volume, ps_out1
            psq_st      ps_out0, 0(outbuffer), 0, 0
            ps_merge11  ps_in3, ps_in23, ps_in23
            ps_madd     ps_out2, ps_in2, ps_volume, ps_out2
            psq_st      ps_out1, 8(outbuffer), 0, 0
            ps_madd     ps_out3, ps_in3, ps_volume, ps_out3
            psq_st      ps_out2, 16(outbuffer), 0, 0
            psq_st      ps_out3, 24(outbuffer), 0, 0
            addi        outbuffer, outbuffer, 32
            cmpw        outbuffer, outbufferend
            bne         loop
        }
    }

    len &= 3;
    float vl = volumel;
    float vr = volumer;
    while ( len ) {
        outbuffer[0] += inbuffer[0] * vl;
        outbuffer[1] += inbuffer[0] * vr;
        inbuffer++;
        outbuffer += 2;
        len--;
    }
} // 0x803571F4

// Stereo to stereo: paired-single loop over 4 frames, then the scalar remainder.
// Wall (one line): the MWCC schedule of the asm block (psq order) and the frsp register choice differ from retail.
// Gormiti DWARF: ps_in0..ps_in3, ps_out0..ps_out3, ps_volume.
void FMOD_DSP_Connection_MixStereoToStereo_SIMD( register float *inbuffer, register float *outbuffer, unsigned int length, register double volumel, register double volumer ) {
    unsigned int len;
    int          count;
    register float ps_in0, ps_in1, ps_in2, ps_in3;
    register float ps_out0, ps_out1, ps_out2, ps_out3;
    register float ps_volume;
    register unsigned int outbufferend;

    len          = length;
    count        = len >> 2;
    outbufferend = (unsigned int)outbuffer + ( count << 5 );

    if ( count ) {
        asm {
            ps_merge00  ps_volume, volumel, volumer
        loop:
            psq_l       ps_in0, 0(inbuffer), 0, 0
            psq_l       ps_out0, 0(outbuffer), 0, 0
            psq_l       ps_in1, 8(inbuffer), 0, 0
            psq_l       ps_out1, 8(outbuffer), 0, 0
            ps_madd     ps_out0, ps_in0, ps_volume, ps_out0
            psq_l       ps_in2, 16(inbuffer), 0, 0
            psq_l       ps_in3, 24(inbuffer), 0, 0
            ps_madd     ps_out1, ps_in1, ps_volume, ps_out1
            psq_l       ps_out2, 16(outbuffer), 0, 0
            addi        inbuffer, inbuffer, 32
            psq_l       ps_out3, 24(outbuffer), 0, 0
            ps_madd     ps_out2, ps_in2, ps_volume, ps_out2
            psq_st      ps_out0, 0(outbuffer), 0, 0
            ps_madd     ps_out3, ps_in3, ps_volume, ps_out3
            psq_st      ps_out1, 8(outbuffer), 0, 0
            psq_st      ps_out2, 16(outbuffer), 0, 0
            psq_st      ps_out3, 24(outbuffer), 0, 0
            addi        outbuffer, outbuffer, 32
            cmpw        outbuffer, outbufferend
            bne         loop
        }
    }

    len &= 3;
    float vl = volumel;
    float vr = volumer;
    while ( len ) {
        outbuffer[0] += inbuffer[0] * vl;
        outbuffer[1] += inbuffer[1] * vr;
        inbuffer += 2;
        outbuffer += 2;
        len--;
    }
} // 0x8035736C

// Mono to 5.1: six output channels per frame, each scaled by its channel volume. The 4-frame loop is
// hand-unrolled; each statement reloads inbuffer[] because the outbuffer stores may alias it.
// Gormiti DWARF: l00..l30 (volume0to3), l40, l50 (volume4to1); volume2to5 is not read on this path.
// Wall (one line): retail's single-frame tail is a counted loop with its own early returns (3 loops, 4 returns); compiled as one if, so the f0/f1 temporaries differ (control flow).
void FMOD_DSP_Connection_MixMonoTo5_1_SIMD( float *inbuffer, float *outbuffer, unsigned int length, float *volume0to3, float *volume4to1, float *volume2to5 ) {
    unsigned int len;
    unsigned int count;
    float        l00, l10, l20, l30, l40, l50;

    l00 = volume0to3[0];
    l10 = volume0to3[1];
    l20 = volume0to3[2];
    l30 = volume0to3[3];
    l40 = volume4to1[0];
    l50 = volume4to1[1];

    len = length >> 2;
    while ( len-- ) {
        outbuffer[0]  += inbuffer[0] * l00;
        outbuffer[1]  += inbuffer[0] * l10;
        outbuffer[2]  += inbuffer[0] * l20;
        outbuffer[3]  += inbuffer[0] * l30;
        outbuffer[4]  += inbuffer[0] * l40;
        outbuffer[5]  += inbuffer[0] * l50;
        outbuffer[6]  += inbuffer[1] * l00;
        outbuffer[7]  += inbuffer[1] * l10;
        outbuffer[8]  += inbuffer[1] * l20;
        outbuffer[9]  += inbuffer[1] * l30;
        outbuffer[10] += inbuffer[1] * l40;
        outbuffer[11] += inbuffer[1] * l50;
        outbuffer[12] += inbuffer[2] * l00;
        outbuffer[13] += inbuffer[2] * l10;
        outbuffer[14] += inbuffer[2] * l20;
        outbuffer[15] += inbuffer[2] * l30;
        outbuffer[16] += inbuffer[2] * l40;
        outbuffer[17] += inbuffer[2] * l50;
        outbuffer[18] += inbuffer[3] * l00;
        outbuffer[19] += inbuffer[3] * l10;
        outbuffer[20] += inbuffer[3] * l20;
        outbuffer[21] += inbuffer[3] * l30;
        outbuffer[22] += inbuffer[3] * l40;
        outbuffer[23] += inbuffer[3] * l50;
        inbuffer += 4;
        outbuffer += 24;
    }

    len = length & 3;
    if ( len == 0 ) {
        return;
    }
    for ( count = len >> 1; count; count-- ) {
        outbuffer[0]  += inbuffer[0] * l00;
        outbuffer[1]  += inbuffer[0] * l10;
        outbuffer[2]  += inbuffer[0] * l20;
        outbuffer[3]  += inbuffer[0] * l30;
        outbuffer[4]  += inbuffer[0] * l40;
        outbuffer[5]  += inbuffer[0] * l50;
        outbuffer[6]  += inbuffer[1] * l00;
        outbuffer[7]  += inbuffer[1] * l10;
        outbuffer[8]  += inbuffer[1] * l20;
        outbuffer[9]  += inbuffer[1] * l30;
        outbuffer[10] += inbuffer[1] * l40;
        outbuffer[11] += inbuffer[1] * l50;
        inbuffer += 2;
        outbuffer += 12;
    }
    len &= 1;
    if ( len ) {
        outbuffer[0] += inbuffer[0] * l00;
        outbuffer[1] += inbuffer[0] * l10;
        outbuffer[2] += inbuffer[0] * l20;
        outbuffer[3] += inbuffer[0] * l30;
        outbuffer[4] += inbuffer[0] * l40;
        outbuffer[5] += inbuffer[0] * l50;
    }
} // 0x803574DC

// 5.1 to 5.1: the six channels of each frame are scaled and added into the output. Channels 0-3 of
// each frame use volume0to3[4] and channels 4-5 use volume4to1[4] (Gormiti DWARF: l00..l55).
// Each sample is read once, from its own channel slot, so the loop needs no reload of inbuffer.
void FMOD_DSP_Connection_Mix5_1To5_1_SIMD( float *inbuffer, float *outbuffer, unsigned int length, float *volume0to3, float *volume4to1, float *volume2to5 ) {
    unsigned int len;
    unsigned int count;
    float        front, rear;

    front = volume0to3[4];
    rear  = volume4to1[4];

    len = length >> 2;
    while ( len-- ) {
        outbuffer[0]  += inbuffer[0]  * front;
        outbuffer[1]  += inbuffer[1]  * front;
        outbuffer[2]  += inbuffer[2]  * front;
        outbuffer[3]  += inbuffer[3]  * front;
        outbuffer[4]  += inbuffer[4]  * rear;
        outbuffer[5]  += inbuffer[5]  * rear;
        outbuffer[6]  += inbuffer[6]  * front;
        outbuffer[7]  += inbuffer[7]  * front;
        outbuffer[8]  += inbuffer[8]  * front;
        outbuffer[9]  += inbuffer[9]  * front;
        outbuffer[10] += inbuffer[10] * rear;
        outbuffer[11] += inbuffer[11] * rear;
        outbuffer[12] += inbuffer[12] * front;
        outbuffer[13] += inbuffer[13] * front;
        outbuffer[14] += inbuffer[14] * front;
        outbuffer[15] += inbuffer[15] * front;
        outbuffer[16] += inbuffer[16] * rear;
        outbuffer[17] += inbuffer[17] * rear;
        outbuffer[18] += inbuffer[18] * front;
        outbuffer[19] += inbuffer[19] * front;
        outbuffer[20] += inbuffer[20] * front;
        outbuffer[21] += inbuffer[21] * front;
        outbuffer[22] += inbuffer[22] * rear;
        outbuffer[23] += inbuffer[23] * rear;
        inbuffer += 24;
        outbuffer += 24;
    }

    len = length & 3;
    if ( len == 0 ) {
        return;
    }
    for ( count = len >> 1; count; count-- ) {
        outbuffer[0]  += inbuffer[0]  * front;
        outbuffer[1]  += inbuffer[1]  * front;
        outbuffer[2]  += inbuffer[2]  * front;
        outbuffer[3]  += inbuffer[3]  * front;
        outbuffer[4]  += inbuffer[4]  * rear;
        outbuffer[5]  += inbuffer[5]  * rear;
        outbuffer[6]  += inbuffer[6]  * front;
        outbuffer[7]  += inbuffer[7]  * front;
        outbuffer[8]  += inbuffer[8]  * front;
        outbuffer[9]  += inbuffer[9]  * front;
        outbuffer[10] += inbuffer[10] * rear;
        outbuffer[11] += inbuffer[11] * rear;
        inbuffer += 12;
        outbuffer += 12;
    }
    len &= 1;
    if ( len ) {
        outbuffer[0] += inbuffer[0] * front;
        outbuffer[1] += inbuffer[1] * front;
        outbuffer[2] += inbuffer[2] * front;
        outbuffer[3] += inbuffer[3] * front;
        outbuffer[4] += inbuffer[4] * rear;
        outbuffer[5] += inbuffer[5] * rear;
    }
} // 0x80357894

} // extern "C"
