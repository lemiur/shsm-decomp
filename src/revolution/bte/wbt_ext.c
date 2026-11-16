// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/wbt_ext.c (Broadcom BTE, Apache-2.0)

/******************************************************************************
 *
 *  Copyright (C) 2003-2012 Broadcom Corporation
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

#include <string.h> // memset, strcpy

#include <revolution/bte/data_types.h>

#include <revolution/bte/hcidefs.h> // LMP_COMPID_BROADCOM
#include <revolution/bte/sdp_api.h>
#include <revolution/bte/sdpdefs.h>
#include <revolution/bte/wbt_api.h>

BOOLEAN WBT_ExtCreateRecord(void)
{
    tSDP_DI_GET_RECORD get_device_info;
    UINT32 sdp_record_handle = 0;
    tSDP_DI_RECORD *p_set_info;
    UINT8 temp_u16[2];
    UINT16 list = UUID_SERVCLASS_PUBLIC_BROWSE_GROUP;

    if (SDP_GetLocalDiRecord(&get_device_info, &sdp_record_handle)
        != SDP_SUCCESS)
    {
        p_set_info = &get_device_info.rec;

        memset(p_set_info, 0, sizeof *p_set_info);
        p_set_info->vendor = LMP_COMPID_BROADCOM;
        p_set_info->vendor_id_source = DI_VENDOR_ID_SOURCE_BTSIG;
        p_set_info->primary_record = TRUE;

        if (SDP_SetLocalDiRecord(p_set_info, &sdp_record_handle) != SDP_SUCCESS)
        {
            sdp_record_handle = 0;
            return FALSE;
        }
    }

    SDP_AddUuidSequence(sdp_record_handle, ATTR_ID_BROWSE_GROUP_LIST, 1, &list);

    temp_u16[0] = 0;
    temp_u16[1] = 1;
    SDP_AddAttribute(sdp_record_handle, ATTR_ID_EXT_BRCM_VERSION,
                     UINT_DESC_TYPE, sizeof temp_u16, temp_u16);

    return TRUE;
} // 0x8025F654

BOOLEAN WBT_ExtAddPinCode(void)
{
    UINT32 sdp_record_handle = 0;
    UINT8 temp_u32[4];
    tSDP_DI_GET_RECORD get_device_info;

    if (SDP_GetLocalDiRecord(&get_device_info, &sdp_record_handle)
        == SDP_SUCCESS)
    {
        temp_u32[0] = 0;
        temp_u32[1] = 0;
        temp_u32[2] = 0;
        temp_u32[3] = 0;

        SDP_AddAttribute(sdp_record_handle, ATTR_ID_EXT_PIN_CODE, 1,
                         sizeof temp_u32, temp_u32);

        return TRUE;
    }
    else
    {
        return FALSE;
    }
} // no copy in retail (dead-stripped)

UINT32 wbt_sdp_show_ext(UINT8 scn, char *service_name, UINT8 pin_code_ext,
                        UINT8 active_sync_ext)
{
    tSDP_DI_RECORD device_info;
    UINT32 sdp_record_handle;

    device_info.client_executable_url[0] = '\0';
    device_info.documentation_url[0] = '\0';
    strcpy(device_info.service_description, service_name);
    device_info.vendor = LMP_COMPID_BROADCOM;
    device_info.vendor_id_source = DI_VENDOR_ID_SOURCE_BTSIG;
    device_info.product = 0x1234; // TODO: which product is this
    device_info.version = 1; // TODO: should this be a define or no
    device_info.primary_record = TRUE;

    if (SDP_SetLocalDiRecord(&device_info, &sdp_record_handle) != SDP_SUCCESS)
        sdp_record_handle = 0;

    return sdp_record_handle;
} // no copy in retail (dead-stripped)
