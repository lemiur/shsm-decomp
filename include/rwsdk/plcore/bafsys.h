#ifndef RWSDK_PLCORE_BAFSYS_H
#define RWSDK_PLCORE_BAFSYS_H

#include <rwsdk/rwtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The file functions the core's streams call directly, in the roles of the C library's stdio
 * calls. They are defined in rtfsmgr.c (RtFileSystem manager).
 */
void* RwFopen(const RwChar* name, const RwChar* access);                   /* fopen */
RwInt32 RwFclose(void* file);                                                /* fclose */
size_t RwFread(void* buffer, size_t size, size_t count, void* file);       /* fread */
size_t RwFwrite(const void* buffer, size_t size, size_t count, void* file); /* fwrite */
RwInt32 RwFseek(void* file, long offset, RwInt32 origin);                   /* fseek */
RwInt32 RwFeof(void* file);                                                /* feof */
long RwFtell(void* file);                                                   /* ftell */
RwBool RwFexist(const RwChar* name);                                        /* whether a file can be opened */

#ifdef __cplusplus
}
#endif

#endif
