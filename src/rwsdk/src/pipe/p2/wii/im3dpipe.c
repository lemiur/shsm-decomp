// Unit name and boundary: Gormiti link map (RwCore.a im3dpipe)
// Names, parameters and locals: Gormiti DWARF (im3dpipe.c)
/*
 * im3dpipe.c: the platform's immediate-mode pipelines. One transform pipeline and one render
 * pipeline, shared by every primitive type, are built when the immediate-mode module opens and
 * become the defaults of RwIm3DSetTransformPipeline and RwIm3DSetRenderPipeline.
 */
#include <rwsdk/pipe/p2/wii/im3dpipe.h>
#include <rwsdk/pipe/p2/wii/nodedolphinsubmitnolight.h>
#include <rwsdk/pipe/p2/baim3d.h>
#include <rwsdk/pipe/p2/p2core.h>
#include <rwsdk/pipe/p2/p2define.h>

/*
 * Builds the platform's transform pipeline (one node, the vertex instancing node), keeps it in
 * globalPipe and makes it the transform pipeline in use. A pipeline that could not be completed is
 * destroyed; if only the unlock failed the original destroys NULL.
 */
RwBool _rwIm3DCreatePlatformTransformPipeline(RxPipeline** globalPipe) {
    RxPipeline* pipe;

    pipe = RxPipelineCreate();
    if (pipe) {
        RxPipeline* lpipe;

        pipe->pluginId = rwID_COREPLUGIN;

        lpipe = RxPipelineLock(pipe);
        if (lpipe) {
            lpipe = RxLockedPipeAddFragment(lpipe, NULL,
                                            RxNodeDefinitionGetGameCubeImmInstance(), NULL);
            pipe = RxLockedPipeUnlock(lpipe);
            if (pipe) {
                *globalPipe = pipe;
                RwIm3DSetTransformPipeline(pipe);
                return TRUE;
            }
        }

        _rxPipelineDestroy(pipe);
    }

    return FALSE;
} // 0x8029F0F4

/* Releases the platform's transform pipeline and falls back to none. */
void _rwIm3DDestroyPlatformTransformPipeline(RxPipeline** globalPipe) {
    RwIm3DSetTransformPipeline(NULL);

    if (*globalPipe) {
        _rxPipelineDestroy(*globalPipe);
        *globalPipe = NULL;
    }
}  // 0x8029F19C

/*
 * Releases the platform's render pipeline (one pipeline serves the five primitive types a
 * pipeline is built for) and falls back to none.
 */
void _rwIm3DDestroyPlatformRenderPipelines(rwIm3DRenderPipelines* globalPipes) {
    RwIm3DSetRenderPipeline(NULL, rwPRIMTYPETRILIST);
    RwIm3DSetRenderPipeline(NULL, rwPRIMTYPETRIFAN);
    RwIm3DSetRenderPipeline(NULL, rwPRIMTYPETRISTRIP);
    RwIm3DSetRenderPipeline(NULL, rwPRIMTYPELINELIST);
    RwIm3DSetRenderPipeline(NULL, rwPRIMTYPEPOLYLINE);

    if (globalPipes->triList) {
        _rxPipelineDestroy(globalPipes->triList);
    }

    globalPipes->triList = NULL;
    globalPipes->triFan = NULL;
    globalPipes->triStrip = NULL;
    globalPipes->lineList = NULL;
    globalPipes->polyLine = NULL;
} // 0x8029F1E4

/*
 * Builds the platform's render pipeline (one node, the unlit submit node), keeps it in globalPipes
 * as the pipeline of each primitive type but points and sprites, and makes it the render pipeline
 * in use for them. A pipeline that could not be completed is destroyed; if only the unlock failed
 * the original destroys NULL.
 */
RwBool _rwIm3DCreatePlatformRenderPipelines(rwIm3DRenderPipelines* globalPipes) {
    RxPipeline* pipe;

    pipe = RxPipelineCreate();
    if (pipe) {
        RxPipeline* lpipe;

        pipe->pluginId = rwID_COREPLUGIN;

        lpipe = RxPipelineLock(pipe);
        if (lpipe) {
            lpipe = RxLockedPipeAddFragment(lpipe, NULL,
                                            RxNodeDefinitionGetGameCubeSubmitNoLight(), NULL);
            pipe = RxLockedPipeUnlock(lpipe);
            if (pipe) {
                globalPipes->triList = pipe;
                globalPipes->triFan = pipe;
                globalPipes->triStrip = pipe;
                globalPipes->lineList = pipe;
                globalPipes->polyLine = pipe;

                RwIm3DSetRenderPipeline(pipe, rwPRIMTYPETRILIST);
                RwIm3DSetRenderPipeline(pipe, rwPRIMTYPETRIFAN);
                RwIm3DSetRenderPipeline(pipe, rwPRIMTYPETRISTRIP);
                RwIm3DSetRenderPipeline(pipe, rwPRIMTYPELINELIST);
                RwIm3DSetRenderPipeline(pipe, rwPRIMTYPEPOLYLINE);
                return TRUE;
            }
        }

        _rxPipelineDestroy(pipe);
    }

    return FALSE;
} // 0x8029F270
