/***************************************************************************//**
 * @file
 * @brief Core application logic.
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
#include "em_common.h"
#include "sl_app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"

#include "filters.h"
#include "rsp_queue.h"

#include <stdlib.h>
#include <string.h>
#include <log.h>

static void on_system_boot(void);
static void scanner_on_event(sl_bt_msg_t *evt);

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  scanner_on_event(evt);
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Print boot message.
      LOGD("Bluetooth stack booted: v%d.%d.%d-b%d\n",
                 evt->data.evt_system_boot.major,
                 evt->data.evt_system_boot.minor,
                 evt->data.evt_system_boot.patch,
                 evt->data.evt_system_boot.build);
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to get Bluetooth address\n",
                    (int)sc);

      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];
      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to write attribute\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

/******************************************************************
 * Extended advertising example - scanner with LRU
 * ***************************************************************/

/*
 * REFRESH_PERIOD - How long to update serial output once.
 *
 * If an advertisement is not scanned for the REFRESH_PERIOD * MISS_CNT period,
 * it's considered to be not presented anymore, will be removed from the queue.
 */
#define MISS_CNT                            (5)
#define REFRESH_PERIOD                      (3 * 32768)
#define REFRESH_TIMER_ID                    (5)

static int counter = 0;
static rsp_queue_t rsp_queue = { 0 };

static void on_rsp_recv(const sl_bt_evt_scanner_scan_report_t *rsp)
{
  rsp_t *r;

  r = find_rsp(&rsp_queue, rsp);
  /* If not in queue, insert it to the queue */
  if (r) {
    head_item(&rsp_queue, r);
  } else {
    insert_rsp(&rsp_queue, rsp);
  }
}

/**
 * @brief update_display - Update the serial output periodically.
 */
static void update_display(void)
{
  int ext = -1, conn = -1, scan = -1;
  rsp_t *r;
  LOGD(RTT_CTRL_CLEAR);
  LOGD("Scan result ---> Number of ADV = %d\n", rsp_queue.num);
  if (!rsp_queue.head) {
    return;
  }

  r = rsp_queue.head;
  do {
    switch (r->data.packet_type & 0x80) {
      case 0x80:
        ext = 1;
        break;
      case 0:
        ext = 0;
        break;
    }

    switch (r->data.packet_type & 0x07) {
      case 0:
        conn = 1;
        scan = 1;
        break;
      case 1:
        conn = 1;
        scan = 0;
        break;
      case 2:
        conn = 0;
        scan = 1;
        break;
      case 3:
        conn = 0;
        scan = 0;
        break;
      default:
        LOGE("---> ERROR Adv packet type = 0x%02x\n", r->data.packet_type);
        break;
    }

    if (ext == -1 || conn == -1 || scan == -1) {
      /* ERROR */
      rsp_t *tmp = r;
      remove_item(&rsp_queue, tmp);
      r = r->next;
      continue;
    }
    LOGI("---%s---RSSI:%d---Connectable:%s---Scan:%s---%s Addr---\n",
         ext ? "Extended ADV" : "Legacy ADV",
         r->data.rssi,
         conn ? "Y" : "N",
         scan ? "Y" : "N",
         r->data.address_type == 1 ? "Random"
         : r->data.address_type == 0 ? "Public" : "Anonymous");
    if (r->data.address_type != 255) {
        HEX_DUMP_REVS(r->data.address.addr, 6);
    }
    LOGV("---> Payload Data\n");
    HEX_DUMP(r->data.data.data, r->data.data.len);
    LOGN();
    r = r->next;
  } while (r && r != rsp_queue.head);
}

static void period_check(void)
{
  counter++;
  rsp_t *r = rsp_queue.head;

  if (!r) {
    return;
  }
  update_display();

  r = r->prev;  /* Check from the last */

  while (counter - r->counter > MISS_CNT) {
    /* Consider the node doesn't exist, remove it */
    rsp_t *tmp = r;
    r = r->prev;
    remove_item(&rsp_queue, tmp);
    if (!rsp_queue.num) {
      break;
    }
  }
}

static void on_system_boot(void)
{
  sl_status_t sc;
  /* Use the maximum scan window and interval to minimize the impact of channel
   * switching */
  sc = sl_bt_scanner_set_timing(sl_bt_gap_1m_phy,
                                                0xFFFF,
                                                0xFFFF);
  sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to set discovery timing\n",
                      (int)sc);
   /*Passive scanning*/
  sc = sl_bt_scanner_set_mode(sl_bt_gap_1m_phy, 0);
  sl_app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to set scanner mode\n",
                        (int)sc);
   /*Start discovering*/
  sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, scanner_discover_generic);
  sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to start discovery\n",
                      (int)sc);
   /*Start refreshing timer*/
  sc = sl_bt_system_set_soft_timer(REFRESH_PERIOD, REFRESH_TIMER_ID, 0);
  sl_app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to set soft-timer\n",
                        (int)sc);
  LOGD("Scanning and timer started.\n");
}

static void scanner_on_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      on_system_boot();
      break;
    case sl_bt_evt_system_soft_timer_id:
      switch (evt->data.evt_system_soft_timer.handle) {
        case REFRESH_TIMER_ID:
          period_check();
          break;
        default:
          break;
      }
      break;
    case sl_bt_evt_scanner_scan_report_id:
      if (run_filters(&evt->data.evt_scanner_scan_report)) {
        on_rsp_recv(&evt->data.evt_scanner_scan_report);
      }
      break;
  }
}
