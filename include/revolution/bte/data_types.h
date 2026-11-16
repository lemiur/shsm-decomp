// Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/private/bte/data_types.h (Broadcom BTE, Apache-2.0)
#ifndef BTE_DATA_TYPES_H
#define BTE_DATA_TYPES_H

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * gki/linux/data_types.h
 */

/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/* Includes changes by muff1n1634 and the mscharged-decomp contributors; adapted for this project. */

#include <revolution/types.h>

/* TRUE and FALSE come from <revolution/types.h>. */

#define BCM_STRNCPY_S(x1, x2, x3, x4) strncpy(x1, x3, x4)

#ifdef __cplusplus
extern "C" {
#endif

typedef s8 INT8;
typedef s16 INT16;
typedef s32 INT32;

typedef u8 UINT8;
typedef u16 UINT16;
typedef u32 UINT32;

typedef u8 BOOLEAN;

#ifdef __cplusplus
}
#endif

#endif // BTE_DATA_TYPES_H
