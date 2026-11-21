// unit name unconfirmed: no string or class-name evidence names this file; rename it when
// evidence appears and keep the open item in docs/exceptions.md.

#include <game/unk_8009C234.h>

/*
====================
CreateVideoDecodeThread
====================
*/
// Not exact: the thread address and the stack top are scheduled differently (register/schedule),
// and the priority copy sits earlier in retail's argument order. The control flow is aligned.
// 0x8009C234
extern "C" BOOL CreateVideoDecodeThread( OSPriority priority, void* param ) {
    unk_DecodeThreadContext* context = &lbl_8046D4D0;

    if ( param != NULL ) {
        if ( !OSCreateThread( &context->thread, VideoDecoderForOnMemory, param,
                              context->stack + sizeof( context->stack ), sizeof( context->stack ),
                              priority, OS_THREAD_ATTR_DETACH ) ) {
            return FALSE;
        }
    } else {
        if ( !OSCreateThread( &context->thread, VideoDecoder, NULL,
                              context->stack + sizeof( context->stack ), sizeof( context->stack ),
                              priority, OS_THREAD_ATTR_DETACH ) ) {
            return FALSE;
        }
    }

    OSInitMessageQueue( &context->messageQueue[0], context->messageBuffer[0], 3 );
    OSInitMessageQueue( &context->messageQueue[1], context->messageBuffer[1], 3 );
    lbl_805297A0 = TRUE;
    lbl_805297A4 = TRUE;
    return TRUE;
}
