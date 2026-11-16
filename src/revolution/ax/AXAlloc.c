// Reference: doldecomp/ogws src/revolution/AX/AXAlloc.c (the Quit function follows the later SDK, as in retail)
#include <revolution/ax.h>
#include <revolution/ax/__ax.h>
#include <revolution/os.h>

#define STACK_MAX (AX_PRIORITY_MAX + 1)

static AXVPB* __AXStackHead[STACK_MAX];
static AXVPB* __AXStackTail[STACK_MAX];
static AXVPB* __AXCallbackStack = NULL;

/*
 * __AXGetStackHead
 *
 * Returns the first voice of a priority stack.
 */
AXVPB* __AXGetStackHead(u32 prio) {
    return __AXStackHead[prio];
} // 0x8022D0F0

/*
 * __AXServiceCallbackStack
 *
 * Runs the callbacks of the voices the DSP has finished with and returns those voices to the free stack.
 */
void __AXServiceCallbackStack(void) {
    AXVPB* it = __AXPopCallbackStack();

    while (it != NULL) {
        if (it->priority > AX_PRIORITY_FREE) {
            if (it->callback != NULL) {
                it->callback(it);
            }

            __AXRemoveFromStack(it);
            __AXPushFreeStack(it);
        }

        it = __AXPopCallbackStack();
    }
} // 0x8022D110

/*
 * __AXInitVoiceStacks
 *
 * Empties the callback stack and every priority stack.
 */
void __AXInitVoiceStacks(void) {
    u32 prio;

    __AXCallbackStack = NULL;

    for (prio = 0; prio < STACK_MAX; prio++) {
        __AXStackHead[prio] = __AXStackTail[prio] = NULL;
    }
}

/*
 * __AXAllocInit
 *
 * Initialises the voice allocator.
 */
void __AXAllocInit(void) {
    __AXInitVoiceStacks();
} // 0x8022D1C0

/*
 * __AXAllocQuit
 *
 * Releases the voice allocator (invented name).
 */
void __AXAllocQuit(void) {
    __AXInitVoiceStacks();
} // 0x8022D270

/*
 * __AXPushFreeStack
 *
 * Puts a voice at the head of the free stack.
 */
void __AXPushFreeStack(AXVPB* vpb) {
    vpb->next = __AXStackHead[AX_PRIORITY_FREE];
    __AXStackHead[AX_PRIORITY_FREE] = vpb;
    vpb->priority = AX_PRIORITY_FREE;
} // 0x8022D320

/*
 * __AXPopFreeStack
 *
 * Takes the first voice off the free stack, or returns NULL when none is free.
 */
AXVPB* __AXPopFreeStack(void) {
    AXVPB* head = __AXStackHead[AX_PRIORITY_FREE];

    if (head != NULL) {
        __AXStackHead[AX_PRIORITY_FREE] = head->next;
    }

    return head;
}

/*
 * __AXPushCallbackStack
 *
 * Queues a voice for its callback.
 */
void __AXPushCallbackStack(AXVPB* vpb) {
    vpb->next1 = __AXCallbackStack;
    __AXCallbackStack = vpb;
} // 0x8022D340

/*
 * __AXPopCallbackStack
 *
 * Takes the first voice off the callback stack, or returns NULL when it is empty.
 */
AXVPB* __AXPopCallbackStack(void) {
    AXVPB* head = __AXCallbackStack;

    if (head != NULL) {
        __AXCallbackStack = head->next1;
    }

    return head;
}

/*
 * __AXRemoveFromStack
 *
 * Unlinks a voice from the priority stack it is in.
 */
void __AXRemoveFromStack(AXVPB* vpb) {
    u32 prio = vpb->priority;
    AXVPB* head = __AXStackHead[prio];
    AXVPB* tail = __AXStackTail[prio];

    if (head == tail) {
        __AXStackTail[prio] = NULL;
        __AXStackHead[prio] = NULL;
    } else if (vpb == head) {
        __AXStackHead[prio] = vpb->next;
        __AXStackHead[prio]->prev = NULL;
    } else if (vpb == tail) {
        __AXStackTail[prio] = vpb->prev;
        __AXStackTail[prio]->next = NULL;
    } else {
        head = vpb->prev;
        tail = vpb->next;
        head->next = tail;
        tail->prev = head;
    }
} // 0x8022D350

/*
 * __AXPushStackHead
 *
 * Puts a voice at the head of a priority stack.
 */
void __AXPushStackHead(AXVPB* vpb, u32 prio) {
    vpb->next = __AXStackHead[prio];
    vpb->prev = NULL;

    if (vpb->next != NULL) {
        __AXStackHead[prio]->prev = vpb;
        __AXStackHead[prio] = vpb;
    } else {
        __AXStackTail[prio] = vpb;
        __AXStackHead[prio] = vpb;
    }

    vpb->priority = prio;
}

/*
 * __AXPopStackFromBottom
 *
 * Takes the last voice off a priority stack, or returns NULL when it is empty.
 */
AXVPB* __AXPopStackFromBottom(u32 prio) {
    AXVPB* vpb = NULL;

    if (__AXStackHead[prio] != NULL) {
        if (__AXStackHead[prio] == __AXStackTail[prio]) {
            vpb = __AXStackHead[prio];
            __AXStackTail[prio] = NULL;
            __AXStackHead[prio] = NULL;
        } else if (__AXStackTail[prio] != NULL) {
            vpb = __AXStackTail[prio];
            __AXStackTail[prio] = vpb->prev;
            __AXStackTail[prio]->next = NULL;
        }
    }

    return vpb;
}

/*
 * AXFreeVoice
 *
 * Releases a voice: takes it off its priority stack, silences it if it is running and returns it to the free stack.
 */
void AXFreeVoice(AXVPB* vpb) {
    BOOL enabled = OSDisableInterrupts();

    __AXRemoveFromStack(vpb);

    if (vpb->pb.state == AX_VOICE_RUN) {
        vpb->depop = TRUE;
    }

    __AXSetPBDefault(vpb);
    __AXPushFreeStack(vpb);

    OSRestoreInterrupts(enabled);
} // 0x8022D3E0

/*
 * AXAcquireVoice
 *
 * Takes a voice from the free stack or, failing that, steals the oldest voice of a lower priority, and puts it
 * on the given priority stack with the given callback.
 */
AXVPB* AXAcquireVoice(u32 prio, AXVoiceCallback callback, u32 userContext) {
    BOOL enabled = OSDisableInterrupts();
    AXVPB* vpb = __AXPopFreeStack();

    if (vpb == NULL) {
        u32 i;

        for (i = AX_PRIORITY_MIN; i < prio; i++) {
            vpb = __AXPopStackFromBottom(i);

            if (vpb != NULL) {
                if (vpb->pb.state == AX_VOICE_RUN) {
                    vpb->depop = TRUE;
                }

                if (vpb->callback != NULL) {
                    vpb->callback(vpb);
                }

                break;
            }
        }
    }

    if (vpb != NULL) {
        __AXPushStackHead(vpb, prio);
        vpb->callback = callback;
        vpb->userContext = userContext;
        __AXSetPBDefault(vpb);
    }

    OSRestoreInterrupts(enabled);
    return vpb;
} // 0x8022D460

/*
 * AXSetVoicePriority
 *
 * Moves a voice to the head of another priority stack.
 */
void AXSetVoicePriority(AXVPB* vpb, u32 prio) {
    BOOL enabled = OSDisableInterrupts();

    __AXRemoveFromStack(vpb);
    __AXPushStackHead(vpb, prio);

    OSRestoreInterrupts(enabled);
}
