// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/wud/R4QK01/WUDHidHost.c
#include <string.h>

#include <revolution/os.h>
#include <revolution/wud/__wud.h>

#define DEBUGPrint WUD_DEBUGPrint

#define DEV_IS_NOT_WBC(info) (!WUDIsLinkedWBC() || (WUDIsLinkedWBC() && WUD_DEV_NAME_IS_CNT((info)->conf.devName)))

/*
 * WUDHidHostCallback
 *
 * Event callback of the BTA HID host: tracks connection state, the handle tables and the buffer status.
 */
void WUDHidHostCallback(tBTA_HH_EVT event, tBTA_HH* pData) {
    WUDControlBlock* p = &_wcb;
    WUDDevInfo* pInfo;
    tBTA_HH_CONN* pConn;
    tBTA_HH_CBDATA* pCbData;
    tBTA_HH_ACL_QUEUE_INFO* pQueueInfo;
    int i;

    switch (event) {
    case BTA_HH_ENABLE_EVT: {
        DEBUGPrint("BTA_HH_ENABLE_EVT\n");
        p->hhFlags |= 1;
        break;
    }
    case BTA_HH_DISABLE_EVT: {
        DEBUGPrint("BTA_HH_DISABLE_EVT\n");
        break;
    }
    case BTA_HH_OPEN_EVT: {
        pConn = &pData->conn;

        DEBUGPrint("BTA_HH_OPEN_EVT\n");
        DEBUGPrint("handle: %d, addr: %02x:%02x:%02x:%02x:%02x:%02x\n", pConn->handle, pConn->bda[0], pConn->bda[1], pConn->bda[2], pConn->bda[3],
                   pConn->bda[4], pConn->bda[5]);

        if (pConn->status == BTA_HH_OK) {
            pInfo = WUDiGetDiscoverDevice();

            if (WUD_BDCMP(pInfo->devAddr, pConn->bda) != 0) {
                pInfo = WUDiGetDevInfo(pConn->bda);
            }

            switch (pInfo->status) {
                case 12: {
                    p->syncState = WUD_STATE_SYNC_REGISTER_DEVICE;
                    break;
                }
                case 2: {
                    p->syncState = WUD_STATE_SYNC_COMPLETE;
                    break;
                }
            }

            pInfo->status = 8;
            pInfo->devHandle = pConn->handle;
            p->connectedNum++;

            pInfo = WUDiGetDevInfo(pConn->bda);
            if (pInfo == NULL) {
                pInfo = WUDiGetDiscoverDevice();
            }

            WUDiSetDevAddrForHandle(pConn->handle, pInfo->devAddr);
            WUDiSetQueueSizeForHandle(pConn->handle, 0);
            WUDiSetNotAckNumForHandle(pConn->handle, 0);

            if (pInfo->sync_type == 3 || pInfo->sync_type == 1) {
                WUDiMoveTopSmpDevInfoPtr(pInfo);
            } else if (DEV_IS_NOT_WBC(pInfo)) {
                WUDiMoveTopStdDevInfoPtr(pInfo);
            }

            WUDSetSniffMode(pInfo->devAddr, 8);

            if (p->hidConnCB != NULL) {
                p->hidConnCB(pInfo, TRUE);
            }
        } else {
            DEBUGPrint("error code: %d\n", pConn->status);

            if (p->syncState != WUD_STATE_SYNC_START) {
                pInfo = WUDiGetDiscoverDevice();
                if (WUD_BDCMP(pConn->bda, pInfo->devAddr) == 0 && pInfo->status == 2) {
                    if (WUDiGetDevInfo(pConn->bda) && pConn->status == BTA_HH_ERR_AUTH_FAILED) {
                        WUDiRemoveDevice(pConn->bda);
                        p->linkedNum--;
                    }
                    p->syncState = WUD_STATE_SYNC_ERROR;
                }
            } else {
                if (WUDiGetDevInfo(pConn->bda) && pConn->status == BTA_HH_ERR_AUTH_FAILED) {
                    pInfo = WUDiGetDevInfo(pConn->bda);
                    if (pInfo != NULL) {
                        if (pInfo->sync_type == 3 || pInfo->sync_type == 1) {
                            WUDiMoveBottomSmpDevInfoPtr(pInfo);
                        } else {
                            WUDiMoveBottomStdDevInfoPtr(pInfo);
                        }
                    }
                    WUDiRemoveDevice(pConn->bda);
                    p->linkedNum--;
                }
            }
        }

        break;
    }
    case BTA_HH_CLOSE_EVT: {
        pCbData = &pData->dev_status;

        DEBUGPrint("BTA_HH_CLOSE_EVT\n");

        p->connectedNum--;

        DEBUGPrint("device handle : %d   status = %d\n", pCbData->handle, pCbData->status);

        pInfo = WUDiGetDevInfo(WUDiGetDevAddrForHandle(pCbData->handle));
        if (pInfo != NULL) {
            if (pInfo->sync_type == 3 || pInfo->sync_type == 1) {
                WUDiMoveTopOfDisconnectedSmpDevice(pInfo);
            } else if (DEV_IS_NOT_WBC(pInfo)) {
                WUDiMoveTopOfDisconnectedStdDevice(pInfo);
            }
        }

        WUDiSetDevAddrForHandle(pCbData->handle, NULL);
        WUDiSetQueueSizeForHandle(pCbData->handle, 0);
        WUDiSetNotAckNumForHandle(pCbData->handle, 0);

        if (p->hidConnCB != NULL) {
            p->hidConnCB(pInfo, FALSE);
        }
        break;
    }
    case BTA_HH_ADD_DEV_EVT: {
        pConn = &pData->dev_info;

        DEBUGPrint("BTA_HH_ADD_DEV_EVT\n");

        DEBUGPrint("result: %d, handle: %d, addr: %02x:%02x:%02x:%02x:%02x:%02x\n", pConn->status, pConn->handle, pConn->bda[0], pConn->bda[1],
                   pConn->bda[2], pConn->bda[3], pConn->bda[4], pConn->bda[5]);

        pInfo = WUDiGetDevInfo(pConn->bda);
        pInfo->devHandle = pConn->handle;
        WUDiSetDevAddrForHandle(pConn->handle, pInfo->devAddr);
        WUDiSetQueueSizeForHandle(pConn->handle, 0);
        WUDiSetNotAckNumForHandle(pConn->handle, 0);
        break;
    }
    case BTA_HH_RMV_DEV_EVT: {
        pConn = &pData->dev_info;

        DEBUGPrint("BTA_HH_RMV_DEV_EVT\n");
        DEBUGPrint("result: %d, handle: %d, addr: %02x:%02x:%02x:%02x:%02x:%02x\n", pConn->status, pConn->handle, pConn->bda[0], pConn->bda[1],
                   pConn->bda[2], pConn->bda[3], pConn->bda[4], pConn->bda[5]);

        break;
    }
    case BTA_HH_GET_ACL_Q_EVT: {
        pQueueInfo = &pData->acl_queue_info;

        p->bufferStatus0 = pQueueInfo->num_avail_buffs;
        p->bufferStatus1 = pQueueInfo->num_buffs;

        if (p->linkedNum < pQueueInfo->num_links) {
            OSReport("WARNING: link num count is modified.\n");
            p->linkedNum = pQueueInfo->num_links;
        }

        for (i = 0; i < pQueueInfo->num_links; i++) {
            if (pQueueInfo->queue_info[i].handle < WUD_MAX_DEV_ENTRY) {
                WUDiSetQueueSizeForHandle(pQueueInfo->queue_info[i].handle, pQueueInfo->queue_info[i].num_queued);
                WUDiSetNotAckNumForHandle(pQueueInfo->queue_info[i].handle, pQueueInfo->queue_info[i].num_not_acked);
            }
        }

        break;
    }
    case BTA_HH_SET_RPT_EVT: {
        DEBUGPrint("BTA_HH_SET_RPT_EVT\n");
        break;
    }
    case BTA_HH_GET_RPT_EVT: {
        DEBUGPrint("BTA_HH_GET_RPT_EVT\n");
        break;
    }

    case BTA_HH_SET_PROTO_EVT: {
        DEBUGPrint("BTA_HH_SET_PROTO_EVT\n");
        break;
    }
    case BTA_HH_GET_PROTO_EVT: {
        DEBUGPrint("BTA_HH_GET_PROTO_EVT\n");
        break;
    }
    case BTA_HH_SET_IDLE_EVT: {
        DEBUGPrint("BTA_HH_SET_IDLE_EVT\n");
        break;
    }
    case BTA_HH_GET_IDLE_EVT: {
        DEBUGPrint("BTA_HH_GET_IDLE_EVT\n");
        break;
    }

    case BTA_HH_GET_DSCP_EVT: {
        DEBUGPrint("BTA_HH_GET_DCSP_EVT\n");
        break;
    }
    case BTA_HH_VC_UNPLUG_EVT: {
        DEBUGPrint("BTA_HH_VS_UNPLUG_EVT\n");
        break;
    }
    }
} // 0x802908C0

/*
 * bta_hh_co_data
 *
 * HID host data-in callout: forwards controller reports to the registered receive callback.
 */
void bta_hh_co_data(UINT8 handle, UINT8* pReport, UINT16 len, tBTA_HH_PROTO_MODE mode, UINT8 subClass, UINT8 appId) {
#pragma unused(mode)
#pragma unused(subClass)

    WUDControlBlock* p = &_wcb;

    if (appId == 3) {
        if (p->hidRecvCB != NULL) {
            p->hidRecvCB(handle, pReport, len);
        }
    } else {
        DEBUGPrint("Invalid app_id [%d]\n", appId);
    }
} // 0x80290E40

/*
 * bta_hh_co_open
 *
 * HID host connection-open callout.
 */
void bta_hh_co_open(UINT8 handle, UINT8 subClass, UINT16 attrMask, UINT8 appId) {
#pragma unused(handle)
#pragma unused(subClass)
#pragma unused(attrMask)
#pragma unused(appId)

    DEBUGPrint("bta_hh_co_open()\n");
} // 0x80290E80

/*
 * bta_hh_co_close
 *
 * HID host connection-close callout.
 */
void bta_hh_co_close(UINT8 handle, UINT8 appId) {
#pragma unused(handle)
#pragma unused(appId)

    DEBUGPrint("bta_hh_co_close()\n");
} // 0x80290E90

/*
 * bta_dm_co_get_compress_memory
 *
 * Device-manager callout for compression memory; WUD provides none.
 */
BOOLEAN bta_dm_co_get_compress_memory(tBTA_SYS_ID id, UINT8** ppMemory, UINT32* memorySize) {
#pragma unused(id)
#pragma unused(ppMemory)
#pragma unused(memorySize)

    return FALSE;
} // 0x80290EA0
