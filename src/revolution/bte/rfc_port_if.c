// Reference: yannicksuter/mscharged-decomp src/RVL_SDK/bte/rfc_port_if.c (Broadcom BTE, Apache-2.0)

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/rfcomm/rfc_port_if.c
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

#include <stddef.h>

#include <revolution/bte/bt_target.h>
#include <revolution/bte/bt_types.h>
#include <revolution/bte/data_types.h>

#include <revolution/bte/port_api.h>
#include <revolution/bte/port_int.h>
#include <revolution/bte/rfc_int.h>
#include <revolution/bte/rfcdefs.h>

tRFC_CB rfc_cb;

/*******************************************************************************
**
** Function         RFCOMM_StartReq
**
** Description      This function handles Start Request from the upper layer.
**
*******************************************************************************/
void RFCOMM_StartReq(tRFC_MCB *p_mcb)
{
    rfc_mx_sm_execute(p_mcb, RFC_MX_EVENT_START_REQ, NULL);
} // no copy in retail (dead-stripped)

/*******************************************************************************
**
** Function         RFCOMM_StartRsp
**
** Description      This function handles Start Response from the upper layer.
**
*******************************************************************************/
void RFCOMM_StartRsp(tRFC_MCB *p_mcb, UINT16 result)
{
    rfc_mx_sm_execute(p_mcb, RFC_MX_EVENT_START_RSP, &result);
} // 0x80270598

/*******************************************************************************
**
** Function         RFCOMM_DlcEstablishReq
**
** Description      This function sends a connection request for a DLC.
**
*******************************************************************************/
void RFCOMM_DlcEstablishReq(tRFC_MCB *p_mcb, UINT8 dlci, UINT16 mtu)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_mcb->state != RFC_MX_STATE_CONNECTED)
    {
        PORT_DlcEstablishCnf(p_mcb, dlci, 0, RFCOMM_ERROR);
        return;
    }

    rfc_port_sm_execute(p_port, RFC_EVENT_OPEN, NULL);
} // 0x802705C4

/*******************************************************************************
**
** Function         RFCOMM_DlcEstablishRsp
**
** Description      This function sends a connection response for a DLC.
**
*******************************************************************************/
void RFCOMM_DlcEstablishRsp(tRFC_MCB *p_mcb, UINT8 dlci, UINT16 mtu,
                            UINT16 result)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_mcb->state != RFC_MX_STATE_CONNECTED && result == RFCOMM_SUCCESS)
    {
        PORT_DlcReleaseInd(p_mcb, dlci);
        return;
    }

    rfc_port_sm_execute(p_port, RFC_EVENT_ESTABLISH_RSP, &result);
} // 0x8027062C

/*******************************************************************************
**
** Function         RFCOMM_ParNegReq
**
** Description      This function sends a parameter negotiation request.
**
*******************************************************************************/
void RFCOMM_ParNegReq(tRFC_MCB *p_mcb, UINT8 dlci, UINT16 mtu)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);
    UINT8 flow;
    UINT8 cl;
    UINT8 k;

    if (p_mcb->state != RFC_MX_STATE_CONNECTED)
    {
        p_port->error = PORT_PAR_NEG_FAILED;
        return;
    }

    flow = p_mcb->flow == PORT_FC_UNDEFINED ? PORT_FC_DEFAULT : p_mcb->flow;

    if (flow == PORT_FC_CREDIT)
    {
        cl = RFCOMM_PN_CONV_LAYER_CBFC_I;
        k = p_port->credit_rx_max < RFCOMM_K_MAX ? p_port->credit_rx_max
                                                 : RFCOMM_K_MAX;
        p_port->credit_rx = k;
    }
    else
    {
        cl = RFCOMM_PN_CONV_LAYER_TYPE_1;
        k = 0;
    }

    p_port->rfc.expected_rsp |= RFC_RSP_PN;

    rfc_send_pn(p_mcb, dlci, TRUE, mtu, cl, k);

    rfc_port_timer_start(p_port, RFC_T2_TIMEOUT);
} // 0x8027069C

/*******************************************************************************
**
** Function         RFCOMM_ParNegRsp
**
** Description      This function sends a parameter negotiation response.
**
*******************************************************************************/
void RFCOMM_ParNegRsp(tRFC_MCB *p_mcb, UINT8 dlci, UINT16 mtu, UINT8 cl,
                      UINT8 k)
{
    if (p_mcb->state != RFC_MX_STATE_CONNECTED)
        return;

    rfc_send_pn(p_mcb, dlci, FALSE, mtu, cl, k);
} // 0x8027077C

/*******************************************************************************
**
** Function         RFCOMM_PortNegReq
**
** Description      This function sends a Remote Port Negotiation command.
**
*******************************************************************************/
void RFCOMM_PortNegReq(tRFC_MCB *p_mcb, UINT8 dlci, tPORT_STATE *p_pars)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_mcb->state != RFC_MX_STATE_CONNECTED)
    {
        PORT_PortNegCnf(p_mcb, dlci, NULL, RFCOMM_ERROR);
        return;
    }

    if (!p_pars)
        p_port->rfc.expected_rsp |= RFC_RSP_RPN_REPLY;
    else
        p_port->rfc.expected_rsp |= RFC_RSP_RPN;

    rfc_send_rpn(p_mcb, dlci, TRUE, p_pars, RFCOMM_RPN_PM_MASK);
    rfc_port_timer_start(p_port, RFC_T2_TIMEOUT);
} // 0x802707A4

/*******************************************************************************
**
** Function         RFCOMM_PortNegRsp
**
** Description      This function sends a Remote Port Negotiation response.
**
*******************************************************************************/
void RFCOMM_PortNegRsp(tRFC_MCB *p_mcb, UINT8 dlci, tPORT_STATE *p_pars,
                       UINT16 param_mask)
{
    if (p_mcb->state != RFC_MX_STATE_CONNECTED)
        return;

    rfc_send_rpn(p_mcb, dlci, FALSE, p_pars, param_mask);
} // 0x80270860

/*******************************************************************************
**
** Function         RFCOMM_ControlReq
**
** Description      This function sends a control command (MSC).
**
*******************************************************************************/
void RFCOMM_ControlReq(tRFC_MCB *p_mcb, UINT8 dlci, tPORT_CTRL *p_pars)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_port->state != PORT_STATE_OPENED
        || p_port->rfc.state != RFC_STATE_OPENED)
    {
        return;
    }

    p_port->port_ctrl |= PORT_CTRL_REQ_SENT;

    p_port->rfc.expected_rsp |= RFC_RSP_MSC;

    rfc_send_msc(p_mcb, dlci, TRUE, p_pars);
    rfc_port_timer_start(p_port, RFC_T2_TIMEOUT);
} // 0x80270880

/*******************************************************************************
**
** Function         RFCOMM_FlowReq
**
** Description      This function sends a flow control command (MSC with FC).
**
*******************************************************************************/
void RFCOMM_FlowReq(tRFC_MCB *p_mcb, UINT8 dlci, UINT8 enable)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_port->state != PORT_STATE_OPENED
        || p_port->rfc.state != RFC_STATE_OPENED)
    {
        return;
    }

    p_port->local_ctrl.fc = !enable;

    p_port->rfc.expected_rsp |= RFC_RSP_MSC;

    rfc_send_msc(p_mcb, dlci, TRUE, &p_port->local_ctrl);
    rfc_port_timer_start(p_port, RFC_T2_TIMEOUT);
} // 0x80270924

/*******************************************************************************
**
** Function         RFCOMM_LineStatusReq
**
** Description      This function sends a line status command (RLS).
**
*******************************************************************************/
void RFCOMM_LineStatusReq(tRFC_MCB *p_mcb, UINT8 dlci, UINT8 status)
{
    tPORT *p_port = port_find_mcb_dlci_port(p_mcb, dlci);

    if (p_port->state != PORT_STATE_OPENED
        || p_port->rfc.state != RFC_STATE_OPENED)
    {
        return;
    }

    p_port->rfc.expected_rsp |= RFC_RSP_RLS;

    rfc_send_rls(p_mcb, dlci, TRUE, status);
    rfc_port_timer_start(p_port, RFC_T2_TIMEOUT);
} // 0x802709C8

/*******************************************************************************
**
** Function         RFCOMM_DlcReleaseReq
**
** Description      This function sends a disconnect request for a DLC.
**
*******************************************************************************/
void RFCOMM_DlcReleaseReq(tRFC_MCB *p_mcb, UINT8 dlci)
{
    rfc_port_sm_execute(port_find_mcb_dlci_port(p_mcb, dlci), RFC_EVENT_CLOSE,
                        NULL);
} // 0x80270A60

/*******************************************************************************
**
** Function         RFCOMM_DataReq
**
** Description      This function passes data to the state machine for sending.
**
*******************************************************************************/
void RFCOMM_DataReq(tRFC_MCB *p_mcb, UINT8 dlci, BT_HDR *p_buf)
{
    rfc_port_sm_execute(port_find_mcb_dlci_port(p_mcb, dlci), RFC_EVENT_DATA,
                        p_buf);
} // 0x80270A8C
