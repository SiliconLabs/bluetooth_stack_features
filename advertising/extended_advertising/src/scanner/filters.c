/***************************************************************************//**
 * @file filters.c
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "filters.h"

#define RSSI_THRESHOLD  (-75)

static bd_addr addrs[] = {
  {
    .addr = {  0x9C, 0x31, 0xEF, 0x57, 0x0B, 0x00 }
  }
  /* ... */
};

static const uint8_t dev_cnt = sizeof(addrs) / sizeof(bd_addr);

typedef int (*filter)(const sl_bt_evt_scanner_scan_report_t *rsp);

/* Filters, the advertisement or scan response will only be passed for further
 * process if all the filters below are passed. */
static filter filters[] = {
  rssi_filter,
  /* addr_filter, */
  /* ... */
};
static const uint8_t filter_cnt = sizeof(filters) / sizeof(filter);

int rssi_filter(const sl_bt_evt_scanner_scan_report_t *rsp)
{
  return (rsp->rssi > RSSI_THRESHOLD);
}

int addr_filter(const sl_bt_evt_scanner_scan_report_t *rsp)
{
  for (int i = 0; i < dev_cnt; i++) {
    if (!memcmp(addrs[i].addr, rsp->address.addr, 6)) {
      return 1;
    }
  }
  return 0;
}

int run_filters(const sl_bt_evt_scanner_scan_report_t *rsp)
{
  for (int i = 0; i < filter_cnt; i++) {
    if (filters[i]) {
      if (!filters[i](rsp)) {
        return 0;
      }
    }
  }
  return 1;
}
