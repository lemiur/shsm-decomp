/*
 * fmod_dsp_connection_wii.cpp: the Wii mixing paths of a DSP connection (paired-single loops).
 * Names and layout: Gormiti link map and DWARF (fmodex.a fmod_dsp_connection_wii.obj)
 */

extern "C" {

// ORIGINAL-ASM: asm block of paired-single instructions, scheduled by MWCC (Gormiti DWARF: ps_* register locals).
// Parked at 60%: the asm block matches; open is the tail loop (retail converts volumel/volumer with frsp
// only after the asm block, and reloads inbuffer[0] after each store).
void FMOD_DSP_Connection_MixMonoToStereo_SIMD( register const float *inbuffer, register float *outbuffer, unsigned int length, double volumel, double volumer ) {
    unsigned int len;
    int          count;
    register float ps_ina, ps_inb;
    register float ps_in0, ps_in1, ps_in2, ps_in3;
    register float ps_out0, ps_out1, ps_out2, ps_out3;
    register float ps_volume, ps_volumel, ps_volumer;
    register unsigned int outbufferend;

    ps_volumel   = volumel;
    ps_volumer   = volumer;
    len          = length;
    count        = len >> 2;
    outbufferend = (unsigned int)outbuffer + ( count << 5 );

    if ( count ) {
        asm {
            ps_merge00  ps_volume, ps_volumel, ps_volumer
        loop:
            psq_l       ps_ina, 0(inbuffer), 0, 0
            psq_l       ps_inb, 8(inbuffer), 0, 0
            psq_l       ps_out0, 0(outbuffer), 0, 0
            psq_l       ps_out1, 8(outbuffer), 0, 0
            psq_l       ps_out2, 16(outbuffer), 0, 0
            psq_l       ps_out3, 24(outbuffer), 0, 0
            ps_merge00  ps_in0, ps_ina, ps_ina
            ps_merge11  ps_in1, ps_ina, ps_ina
            ps_merge00  ps_in2, ps_inb, ps_inb
            ps_merge11  ps_in3, ps_inb, ps_inb
            ps_madd     ps_out0, ps_in0, ps_volume, ps_out0
            ps_madd     ps_out1, ps_in1, ps_volume, ps_out1
            psq_st      ps_out0, 0(outbuffer), 0, 0
            ps_madd     ps_out2, ps_in2, ps_volume, ps_out2
            psq_st      ps_out1, 8(outbuffer), 0, 0
            ps_madd     ps_out3, ps_in3, ps_volume, ps_out3
            psq_st      ps_out2, 16(outbuffer), 0, 0
            psq_st      ps_out3, 24(outbuffer), 0, 0
            addi        inbuffer, inbuffer, 16
            addi        outbuffer, outbuffer, 32
            cmpw        outbuffer, outbufferend
            bne         loop
        }
    }

    len &= 3;
    while ( len ) {
        outbuffer[0] += inbuffer[0] * ps_volumel;
        outbuffer[1] += inbuffer[0] * ps_volumer;
        inbuffer++;
        outbuffer += 2;
        len--;
    }
} // 0x803571F4

void FMOD_DSP_Connection_MixStereoToStereo_SIMD() {
    // TODO
} // 0x8035736C

void FMOD_DSP_Connection_MixMonoTo5_1_SIMD() {
    // TODO
} // 0x803574DC

void FMOD_DSP_Connection_Mix5_1To5_1_SIMD() {
    // TODO
} // 0x80357894

} // extern "C"
