#ifndef REVOLUTION_BTE_H
#define REVOLUTION_BTE_H

#include <revolution/bte/bt_types.h>
#include <revolution/bte/bta_api.h>
#include <revolution/bte/bta_dm_co.h>
#include <revolution/bte/bta_hh_api.h>
#include <revolution/bte/bta_hh_co.h>
#include <revolution/bte/bta_sys.h>
#include <revolution/bte/btm_api.h>
#include <revolution/bte/data_types.h>
#include <revolution/bte/gki.h>
#include <revolution/bte/sdp_api.h> // SDP_SetTraceLevel
#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Subset of the Bluetooth stack (BTE/BTA/BTM) interface used by the WUD driver.
 * The stack itself is a separate library.
 * Reference: zeldaret/tp libs/revolution/include/revolution/wpad/bte.h
 * Reference: yannicksuter/mscharged-decomp libs/RVL_SDK/include/private/bte/*.h
 */

/* BTA device manager and system manager: <revolution/bte/bta_api.h>, <revolution/bte/bta_sys.h>. */

void __ntd_set_ohci(UINT8 ohci);
void __ntd_set_pid_vid(int pid, int vid);
int __ntd_get_usb_file_descriptor(void);
UINT32 __ntd_get_allocated_mem_size(void);
UINT8 L2CA_SetTraceLevel(UINT8 new_level);

/* BTM (Bluetooth manager): the public interface is in <revolution/bte/btm_api.h>. WPAD also calls the
 * stack's internal ACL teardown. */
tBTM_STATUS btm_remove_acl(BD_ADDR bd_addr);

/* HID host: <revolution/bte/bta_hh_api.h>; application callouts (implemented in WUDHidHost.c):
 * <revolution/bte/bta_hh_co.h>, <revolution/bte/bta_dm_co.h>. */

#ifdef __cplusplus
}
#endif

#endif
