#ifndef RWSDK_PIPE_P2_P2ALTMDL_H
#define RWSDK_PIPE_P2_P2ALTMDL_H

#include <rwsdk/plcore/batypes.h>
#include <rwsdk/pipe/p2/p2core.h>

/*
 * Empties one cluster. flags and stride are cleared together through one 32-bit store, as the
 * RenderWare header does (data follows them at offset 4); currentData and attributes are kept.
 */
#define CLUSTERZERO(cl)                                                     \
MACRO_START                                                                 \
{                                                                           \
    /* flags, stride */     *(RwUInt32 *) &(cl)->flags = 0U;                \
    /* data */              (cl)->data = NULL;                              \
    /* numAlloced */        (cl)->numAlloced = 0U;                          \
    /* numUsed */           (cl)->numUsed    = 0U;                          \
    /* clusterRef */        (cl)->clusterRef = (struct RxPipelineCluster *)NULL; \
}                                                                           \
MACRO_STOP

/* Frees a cluster's heap data (unless it is external) and empties the cluster, if it is in use. */
#define CLUSTERTERMINATE(cl)                                                \
MACRO_START                                                                 \
{                                                                           \
    if ( (cl)->clusterRef != (struct RxPipelineCluster *)NULL )             \
    {                                                                       \
        if ( (cl)->data != NULL &&                                          \
             !((cl)->flags & rxCLFLAGS_EXTERNAL) )                          \
        {                                                                   \
            RxHeapFree(_rxHeapGlobal, (cl)->data);                          \
        }                                                                   \
                                                                            \
        CLUSTERZERO(cl);                                                    \
    }                                                                       \
}                                                                           \
MACRO_STOP

#endif /* RWSDK_PIPE_P2_P2ALTMDL_H */
