/*
 * jinclude.h
 *
 * IJG JPEG library 6b: the system include files the library needs.
 */

#ifndef JINCLUDE_H
#define JINCLUDE_H

#include "jconfig.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The game's heap allocation entry points, which the memory backend
 * (jmemnobs.c) calls where the IJG original calls malloc and free.
 * Names: Gormiti link map (jpeg.obj). */
void *rw_malloc(size_t size);
void rw_free(void *object);

/* The game reads JPEG data from RenderWare streams: jdatasrc.c calls
 * RwStreamRead where the IJG original calls JFREAD (fread). */
#include <rwsdk/plcore/bastream.h>

/* Memory copying and zeroing. */
#define MEMZERO(target, size) memset((void *) (target), 0, (size_t) (size))
#define MEMCOPY(dest, src, size) memcpy((void *) (dest), (const void *) (src), (size_t) (size))

/* In ANSI C, and indeed any rational implementation, size_t is also the
 * type returned by sizeof(). */
#define SIZEOF(object) ((size_t) sizeof(object))

#endif /* JINCLUDE_H */
