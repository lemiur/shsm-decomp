// Reference: zeldaret/tp libs/revolution/src/dsp/__dsp.h
//
// DSP driver internals shared by dsp.c, dsp_debug.c and dsp_task.c. Each global is defined in dsp_task.c.
#ifndef REVOLUTION_DSP_INTERNAL_H
#define REVOLUTION_DSP_INTERNAL_H

#include <revolution/dsp.h>
#include <revolution/os.h>

#ifdef __cplusplus
extern "C" {
#endif

extern DSPTask* __DSP_first_task;
extern DSPTask* __DSP_last_task;
extern DSPTask* __DSP_curr_task;
extern DSPTask* __DSP_tmp_task;
extern DSPTask* __DSP_rude_task;
extern int __DSP_rude_task_pending;

void __DSPHandler(__OSInterrupt interrupt, OSContext* context);
void __DSP_exec_task(DSPTask* curr, DSPTask* next);
void __DSP_boot_task(DSPTask* task);
void __DSP_insert_task(DSPTask* task);
void __DSP_add_task(DSPTask* task);
void __DSP_remove_task(DSPTask* task);
void __DSP_debug_printf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
