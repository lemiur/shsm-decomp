// Reference: tgsm/pbg include/rwsdk/batextur.h
// Reference: bfbbdecomp/bfbb include/rwsdk/rwcore.h
#ifndef RWSDK_BATEXTUR_H
#define RWSDK_BATEXTUR_H

#include <rwsdk/baimage.h>
#include <rwsdk/baraster.h>
#include <rwsdk/batypehf.h>
#include <rwsdk/plcore/batkreg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The object type of a texture dictionary (RwObject type). */
#define rwTEXDICTIONARY 6

/* RwTextureFilterMode: how a texture is sampled (the low byte of filterAddressing). */
typedef enum RwTextureFilterMode {
    rwFILTERNAFILTERMODE = 0,
    rwFILTERNEAREST = 1,
    rwFILTERLINEAR = 2,
    rwFILTERMIPNEAREST = 3,
    rwFILTERMIPLINEAR = 4,
    rwFILTERLINEARMIPNEAREST = 5,
    rwFILTERLINEARMIPLINEAR = 6,
    rwTEXTUREFILTERMODEFORCEENUMSIZEINT = 0x7FFFFFFF
} RwTextureFilterMode;

/* RwTextureAddressMode: how texture coordinates outside 0..1 are treated (U and V nibbles of filterAddressing). */
typedef enum RwTextureAddressMode {
    rwTEXTUREADDRESSNATEXTUREADDRESS = 0,
    rwTEXTUREADDRESSWRAP = 1,
    rwTEXTUREADDRESSMIRROR = 2,
    rwTEXTUREADDRESSCLAMP = 3,
    rwTEXTUREADDRESSBORDER = 4,
    rwTEXTUREADDRESSFORCEENUMSIZEINT = 0x7FFFFFFF
} RwTextureAddressMode;

/* The fields of filterAddressing (the mask names bfbbdecomp/bfbb rwcore.h's macros use). */
#define rwTEXTUREFILTERMODEMASK 0x000000FF
#define rwTEXTUREADDRESSINGUMASK 0x00000F00
#define rwTEXTUREADDRESSINGVMASK 0x0000F000
#define rwTEXTUREADDRESSINGMASK (rwTEXTUREADDRESSINGUMASK | rwTEXTUREADDRESSINGVMASK)

/* The length of a texture's name and mask name, with the terminator. */
#define rwTEXTUREBASENAMELENGTH 32

/* RwTexDictionary: a named collection of textures; the engine keeps every dictionary in a list. */
typedef struct RwTexDictionary {
    RwObject object;            /* 0x0: type rwTEXDICTIONARY */
    RwLinkList texturesInDict;  /* 0x8: the textures (RwTexture lInDictionary) */
    RwLLLink lInInstance;       /* 0x10: in the texture module's list of dictionaries */
} RwTexDictionary;

/* RwTexture: a raster with a name, a mask name, filtering and addressing, counted references. */
typedef struct RwTexture {
    RwRaster* raster;                           /* 0x0 */
    RwTexDictionary* dict;                      /* 0x4: the dictionary holding it, or NULL */
    RwLLLink lInDictionary;                     /* 0x8: in dict's texturesInDict */
    RwChar name[rwTEXTUREBASENAMELENGTH];       /* 0x10 */
    RwChar mask[rwTEXTUREBASENAMELENGTH];       /* 0x30 */
    RwUInt32 filterAddressing;                  /* 0x50: RwTextureFilterMode, then the U and V address modes */
    RwInt32 refCount;                           /* 0x54 */
} RwTexture;

/* Texture callbacks. */
typedef RwTexture* (*RwTextureCallBackRead)(const RwChar* name, const RwChar* maskName);
/* Looks a texture up by name before RwTextureRead reads it. */
typedef RwTexture* (*RwTextureCallBackFind)(const RwChar* name);
typedef RwTexture* (*RwTextureCallBack)(RwTexture* texture, void* pData);
typedef RwTexDictionary* (*RwTexDictionaryCallBack)(RwTexDictionary* dict, void* data);
typedef RwRaster* (*RwTextureCallBackMipmapGeneration)(RwRaster* raster, RwImage* image);
typedef RwBool (*RwTextureCallBackMipmapName)(RwChar* name, RwChar* maskName, RwUInt8 mipLevel,
                                              RwInt32 format);

#define RwTextureGetRaster(_tex) ((_tex)->raster)
#define RwTextureAddRef(_tex) (((_tex)->refCount++), (_tex))
#define RwTextureGetName(_tex) ((_tex)->name)
#define RwTextureGetMaskName(_tex) ((_tex)->mask)
#define RwTextureGetDictionary(_tex) ((_tex)->dict)

#define RwTextureSetFilterMode(_tex, _filtering)                                        \
    (((_tex)->filterAddressing = ((_tex)->filterAddressing & ~rwTEXTUREFILTERMODEMASK) | \
                                 (((RwUInt32)(_filtering)) & rwTEXTUREFILTERMODEMASK)),  \
     (_tex))

#define RwTextureSetAddressing(_tex, _addressing)                                     \
    (((_tex)->filterAddressing =                                                       \
          ((_tex)->filterAddressing & ~rwTEXTUREADDRESSINGMASK) |                      \
          (((((RwUInt32)(_addressing)) << 8) & rwTEXTUREADDRESSINGUMASK) |             \
           ((((RwUInt32)(_addressing)) << 12) & rwTEXTUREADDRESSINGVMASK))),           \
     (_tex))

/* The U and V address modes alone, as RwTextureSetAddressing sets them together. */
#define RwTextureSetAddressingU(_tex, _addressing)                                     \
    (((_tex)->filterAddressing =                                                       \
          ((_tex)->filterAddressing & ~rwTEXTUREADDRESSINGUMASK) |                     \
          ((((RwUInt32)(_addressing)) << 8) & rwTEXTUREADDRESSINGUMASK)),              \
     (_tex))

#define RwTextureSetAddressingV(_tex, _addressing)                                     \
    (((_tex)->filterAddressing =                                                       \
          ((_tex)->filterAddressing & ~rwTEXTUREADDRESSINGVMASK) |                     \
          ((((RwUInt32)(_addressing)) << 12) & rwTEXTUREADDRESSINGVMASK)),             \
     (_tex))

/* The plugins registered with textures and with texture dictionaries (also streamed by babintex). */
extern RwPluginRegistry textureTKList;
extern RwPluginRegistry texDictTKList;

RwBool RwTextureSetMipmapping(RwBool enable);
RwBool RwTextureGetMipmapping(void);
RwBool RwTextureSetAutoMipmapping(RwBool enable);
RwBool RwTextureGetAutoMipmapping(void);
RwTexture* RwTextureSetRaster(RwTexture* texture, RwRaster* raster);
RwTexDictionary* RwTexDictionaryCreate(void);
RwBool RwTexDictionaryDestroy(RwTexDictionary* dict);
const RwTexDictionary* RwTexDictionaryForAllTextures(const RwTexDictionary* dict, RwTextureCallBack fpCallBack,
                                               void* pData);
RwBool RwTexDictionaryForAllTexDictionaries(RwTexDictionaryCallBack fpCallBack, void* pData);
RwTexture* RwTextureCreate(RwRaster* raster);
RwBool RwTextureDestroy(RwTexture* texture);
RwTexture* RwTextureSetName(RwTexture* texture, const RwChar* name);
RwTexture* RwTextureSetMaskName(RwTexture* texture, const RwChar* maskName);
RwTexture* RwTexDictionaryAddTexture(RwTexDictionary* dict, RwTexture* texture);
RwTexture* RwTexDictionaryRemoveTexture(RwTexture* texture);
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* dict, const RwChar* name);
/* After aap/librw's TexDictionary::setCurrent; the RW spelling follows RwTexDictionaryGetCurrent. */
RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* dict);
RwTexDictionary* RwTexDictionaryGetCurrent(void);
RwTexture* RwTextureRead(const RwChar* name, const RwChar* maskName);
RwInt32 RwTextureRegisterPlugin(RwInt32 size, RwUInt32 pluginID,
                                RwPluginObjectConstructor constructCB,
                                RwPluginObjectDestructor destructCB, RwPluginObjectCopy copyCB);
RwBool RwTextureRasterGenerateMipmaps(RwRaster* raster, RwImage* image);
void* _rwTextureClose(void* instance, RwInt32 offset, RwInt32 size);
void* _rwTextureOpen(void* instance, RwInt32 offset, RwInt32 size);

#ifdef __cplusplus
}
#endif

#endif
