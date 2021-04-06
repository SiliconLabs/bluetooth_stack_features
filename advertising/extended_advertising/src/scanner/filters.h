#ifndef _FILTES_H_
#define _FILTES_H_

#include "sl_bt_api.h"

/* filter response by RSSI */
int rssi_filter(const sl_bt_evt_scanner_scan_report_t *rsp);

/* filter response by Address */
int addr_filter(const sl_bt_evt_scanner_scan_report_t *rsp);

/* running filter */
int run_filters(const sl_bt_evt_scanner_scan_report_t *rsp);

#endif
