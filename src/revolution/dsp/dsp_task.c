// Reference: zeldaret/tp libs/revolution/src/dsp/dsp_task.c
#include <revolution/dsp.h>
#include <revolution/dsp/__dsp.h>
#include <revolution/hardware.h>
#include <revolution/os.h>

// Mail protocol between the driver and the DSP task monitor (invented names; the SDK uses the bare numbers).
#define DSP_MAIL_FROM_INIT    0xDCD10000 // task started
#define DSP_MAIL_FROM_RESUME  0xDCD10001 // task resumed
#define DSP_MAIL_FROM_YIELD   0xDCD10002 // task yielded
#define DSP_MAIL_FROM_DONE    0xDCD10003 // task finished
#define DSP_MAIL_FROM_REQUEST 0xDCD10004 // task request
#define DSP_MAIL_TO_SWITCH    0xCDD10001 // switch to the next task
#define DSP_MAIL_TO_EXIT      0xCDD10002 // no task left
#define DSP_MAIL_TO_CONTINUE  0xCDD10003 // keep running the current task

// Boot sequence mails (invented names).
#define DSP_MAIL_BOOT_READY     0x8071FEED
#define DSP_MAIL_BOOT_IMEM_ADDR 0x80F3A001
#define DSP_MAIL_BOOT_IMEM_DEST 0x80F3C002
#define DSP_MAIL_BOOT_IMEM_LEN  0x80F3A002
#define DSP_MAIL_BOOT_ZERO      0x80F3B002
#define DSP_MAIL_BOOT_VECTOR    0x80F3D001

static u32 t0, t1, t2; // unused in the SDK

DSPTask* __DSP_curr_task;
DSPTask* __DSP_first_task;
DSPTask* __DSP_last_task;
DSPTask* __DSP_tmp_task;
DSPTask* __DSP_rude_task;
int __DSP_rude_task_pending;

/*
 * __DSPHandler
 *
 * DSP interrupt: reads the task monitor mail and schedules the next task.
 */
void __DSPHandler(__OSInterrupt interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 tmp;
    u32 mail;

    tmp = __DSPRegs[DSP_REG_CSR];
    tmp = (tmp & ~0x28) | 0x80;
    __DSPRegs[DSP_REG_CSR] = tmp;

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);

    while (DSPCheckMailFromDSP() == 0) {
        ;
    }

    mail = DSPReadMailFromDSP();
    // A task cancelled while it runs is finished at its next yield.
    if ((__DSP_curr_task->flags & DSP_TASK_FLAG_CANCEL) && mail == DSP_MAIL_FROM_YIELD) {
        mail = DSP_MAIL_FROM_DONE;
    }

    switch (mail) {
    case DSP_MAIL_FROM_INIT:
        __DSP_curr_task->state = 1;
        if (__DSP_curr_task->init_cb != NULL) {
            __DSP_curr_task->init_cb(__DSP_curr_task);
        }
        break;
    case DSP_MAIL_FROM_RESUME:
        __DSP_curr_task->state = 1;
        if (__DSP_curr_task->res_cb != NULL) {
            __DSP_curr_task->res_cb(__DSP_curr_task);
        }
        break;
    case DSP_MAIL_FROM_YIELD:
        if (__DSP_rude_task_pending) {
            if (__DSP_curr_task == __DSP_rude_task) {
                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_CONTINUE);
                __DSP_rude_task = NULL;
                __DSP_rude_task_pending = 0;

                if (__DSP_curr_task->res_cb != NULL) {
                    __DSP_curr_task->res_cb(__DSP_curr_task);
                }
            } else {
                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
                __DSP_exec_task(__DSP_curr_task, __DSP_rude_task);
                __DSP_curr_task->state = 2;
                __DSP_curr_task = __DSP_rude_task;
                __DSP_rude_task = NULL;
                __DSP_rude_task_pending = 0;
            }
        } else {
            if (__DSP_curr_task->next == NULL) {
                if (__DSP_curr_task == __DSP_first_task) {
                    DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_CONTINUE);
                    if (__DSP_curr_task->res_cb != NULL) {
                        __DSP_curr_task->res_cb(__DSP_curr_task);
                    }
                } else {
                    DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
                    __DSP_exec_task(__DSP_curr_task, __DSP_first_task);
                    __DSP_curr_task->state = 2;
                    __DSP_curr_task = __DSP_first_task;
                }
            } else {
                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
                __DSP_exec_task(__DSP_curr_task, __DSP_curr_task->next);
                __DSP_curr_task->state = 2;
                __DSP_curr_task = __DSP_curr_task->next;
            }
        }
        break;
    case DSP_MAIL_FROM_DONE:
        if (__DSP_rude_task_pending) {
            if (__DSP_curr_task != __DSP_rude_task) {
                if (__DSP_curr_task->done_cb != NULL) {
                    __DSP_curr_task->done_cb(__DSP_curr_task);
                }

                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
                __DSP_exec_task(NULL, __DSP_rude_task);
                __DSP_remove_task(__DSP_curr_task);
                __DSP_curr_task = __DSP_rude_task;
                __DSP_rude_task = NULL;
                __DSP_rude_task_pending = 0;
                break;
            }

            __DSP_rude_task = NULL;
            __DSP_rude_task_pending = 0;
        }

        if (__DSP_curr_task->next == NULL) {
            if (__DSP_curr_task == __DSP_first_task) {
                if (__DSP_curr_task->done_cb != NULL) {
                    __DSP_curr_task->done_cb(__DSP_curr_task);
                }

                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_EXIT);
                __DSP_curr_task->state = 3;
                __DSP_remove_task(__DSP_curr_task);
            } else {
                if (__DSP_curr_task->done_cb != NULL) {
                    __DSP_curr_task->done_cb(__DSP_curr_task);
                }

                DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
                __DSP_curr_task->state = 3;
                __DSP_exec_task(NULL, __DSP_first_task);
                __DSP_curr_task = __DSP_first_task;
                __DSP_remove_task(__DSP_last_task);
            }
        } else {
            if (__DSP_curr_task->done_cb != NULL) {
                __DSP_curr_task->done_cb(__DSP_curr_task);
            }

            DSP_SEND_MAIL_SYNC(DSP_MAIL_TO_SWITCH);
            __DSP_curr_task->state = 3;
            __DSP_exec_task(NULL, __DSP_curr_task->next);
            __DSP_curr_task = __DSP_curr_task->next;
            __DSP_remove_task(__DSP_curr_task->prev);
        }
        break;
    case DSP_MAIL_FROM_REQUEST:
        if (__DSP_curr_task->req_cb != NULL) {
            __DSP_curr_task->req_cb(__DSP_curr_task);
        }
        break;
    default:
        break;
    }

    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);
} // 0x80236F00

/*
 * __DSP_exec_task
 *
 * Saves the data memory of the task being switched out (curr, or nothing) and loads the instruction
 * memory and the start or resume state of the next task.
 */
void __DSP_exec_task(DSPTask* curr, DSPTask* next) {
    if (curr != NULL) {
        DSP_SEND_MAIL_SYNC((u32)curr->dram_mmem_addr);
        DSP_SEND_MAIL_SYNC(curr->dram_length);
        DSP_SEND_MAIL_SYNC(curr->dram_addr);
    } else {
        DSP_SEND_MAIL_SYNC(0);
        DSP_SEND_MAIL_SYNC(0);
        DSP_SEND_MAIL_SYNC(0);
    }

    DSP_SEND_MAIL_SYNC((u32)next->iram_mmem_addr);
    DSP_SEND_MAIL_SYNC(next->iram_length);
    DSP_SEND_MAIL_SYNC(next->iram_addr);

    if (next->state == 0) {
        DSP_SEND_MAIL_SYNC(next->dsp_init_vector);
        DSP_SEND_MAIL_SYNC(0);
        DSP_SEND_MAIL_SYNC(0);
        DSP_SEND_MAIL_SYNC(0);
    } else {
        DSP_SEND_MAIL_SYNC(next->dsp_resume_vector);
        DSP_SEND_MAIL_SYNC((u32)next->dram_mmem_addr);
        DSP_SEND_MAIL_SYNC(next->dram_length);
        DSP_SEND_MAIL_SYNC(next->dram_addr);
    }
} // 0x80237330

/*
 * __DSP_boot_task
 *
 * Boots the DSP with the first task: waits for the monitor sync mail, then sends the instruction
 * memory load and the start vector.
 */
void __DSP_boot_task(DSPTask* task) {
    // Retail keeps the one store of the boot mail; only the release-disabled ASSERTMSG1 reads it.
    // volatile: read by a release-disabled assertion, zeldaret/tp libs/revolution/src/dsp/dsp_task.c
    volatile u32 mail;

    while (DSPCheckMailFromDSP() == 0) {
        ;
    }

    mail = DSPReadMailFromDSP();
    ASSERTMSG1(mail == DSP_MAIL_BOOT_READY, "__DSP_boot_task(): Failed to sync DSP on boot! (0x%08X)\n", mail);

    DSP_SEND_MAIL_SYNC(DSP_MAIL_BOOT_IMEM_ADDR);
    DSP_SEND_MAIL_SYNC((u32)task->iram_mmem_addr);
    DSP_SEND_MAIL_SYNC(DSP_MAIL_BOOT_IMEM_DEST);
    DSP_SEND_MAIL_SYNC(task->iram_addr & 0xFFFF);
    DSP_SEND_MAIL_SYNC(DSP_MAIL_BOOT_IMEM_LEN);
    DSP_SEND_MAIL_SYNC(task->iram_length);
    DSP_SEND_MAIL_SYNC(DSP_MAIL_BOOT_ZERO);
    DSP_SEND_MAIL_SYNC(0);
    DSP_SEND_MAIL_SYNC(DSP_MAIL_BOOT_VECTOR);
    DSP_SEND_MAIL_SYNC(task->dsp_init_vector & 0xFFFF);

    __DSP_debug_printf("DSP is booting task: 0x%08X\n", (u32)task);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM MMEM ADDR: 0x%08X\n", (u32)task->iram_mmem_addr);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM DSP ADDR : 0x%08X\n", task->iram_addr);
    __DSP_debug_printf("__DSP_boot_task()  : IRAM LENGTH   : 0x%08X\n", task->iram_length);
    __DSP_debug_printf("__DSP_boot_task()  : DRAM MMEM ADDR: 0x%08X\n", task->dram_length);
    __DSP_debug_printf("__DSP_boot_task()  : Start Vector  : 0x%08X\n", task->dsp_init_vector);
} // 0x802374E0

/*
 * __DSP_insert_task
 *
 * Inserts a task into the list in priority order.
 */
void __DSP_insert_task(DSPTask* task) {
    DSPTask* temp;

    if (__DSP_first_task == NULL) {
        __DSP_curr_task = task;
        __DSP_first_task = __DSP_last_task = task;
        task->next = task->prev = NULL;
        return;
    }

    temp = __DSP_first_task;
    while (temp != NULL) {
        if (task->priority < temp->priority) {
            task->prev = temp->prev;
            temp->prev = task;
            task->next = temp;
            if (task->prev == NULL) {
                __DSP_first_task = task;
            } else {
                task->prev->next = task;
            }
            break;
        }
        temp = temp->next;
    }

    if (temp == NULL) {
        __DSP_last_task->next = task;
        task->next = NULL;
        task->prev = __DSP_last_task;
        __DSP_last_task = task;
    }
} // 0x80237670

/*
 * __DSP_add_task
 *
 * Appends a task to the list (no priority order). Not called in this build.
 */
void __DSP_add_task(DSPTask* task) {
    if (__DSP_last_task == NULL) {
        __DSP_curr_task = task;
        __DSP_last_task = task;
        __DSP_first_task = task;
        task->next = task->prev = NULL;
    } else {
        __DSP_last_task->next = task;
        task->next = NULL;
        task->prev = __DSP_last_task;
        __DSP_last_task = task;
    }

    task->state = 0;
    __DSP_debug_printf("__DSP_add_task() : Added task    : 0x%08X\n", (u32)task);
}

/*
 * __DSP_remove_task
 */
void __DSP_remove_task(DSPTask* task) {
    task->flags = 0;
    task->state = 3;

    if (__DSP_first_task == task) {
        if (task->next != NULL) {
            __DSP_first_task = task->next;
            task->next->prev = NULL;
            return;
        }

        __DSP_first_task = __DSP_last_task = __DSP_curr_task = NULL;
        return;
    }

    if (__DSP_last_task == task) {
        __DSP_last_task = task->prev;
        task->prev->next = NULL;
        __DSP_curr_task = __DSP_first_task;
        return;
    }

    __DSP_curr_task = task->next;
    task->prev->next = task->next;
    task->next->prev = task->prev;
} // 0x80237710
