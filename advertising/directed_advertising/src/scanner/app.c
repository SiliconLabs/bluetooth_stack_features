/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "app_log.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
// Advertiser address
static bd_addr advertiser = {.addr = {0x39, 0x5C, 0xE6, 0x27, 0xFD, 0x84}};
// Connection ID
uint8_t connection = 0;

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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      app_log("Booting...\r\n");

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

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
      app_assert_status(sc);

      app_log("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
               address_type ? "static random" : "public device",
               address.addr[5],
               address.addr[4],
               address.addr[3],
               address.addr[2],
               address.addr[1],
               address.addr[0]);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);

      sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_observation);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("Connected\r\n");
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      connection = 0;
      sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_observation);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    case  sl_bt_evt_scanner_scan_report_id: {
      struct sl_bt_evt_scanner_scan_report_s *scan_resp;
      scan_resp = (struct sl_bt_evt_scanner_scan_report_s *)&(evt->data);
      app_log("RSSI %d, Type %d, Addr 0x%02X%02X%02X%02X%02X%02X, Addr Type %X, Bond %d, msg len: %x, msg: 0x\r\n", scan_resp->rssi, scan_resp->packet_type, scan_resp->address.addr[5], scan_resp->address.addr[4], scan_resp->address.addr[3], scan_resp->address.addr[2], scan_resp->address.addr[1], scan_resp->address.addr[0], scan_resp->address_type, scan_resp->bonding, scan_resp->data.len);

      if(scan_resp->address.addr[0] == advertiser.addr[0] &&
         scan_resp->address.addr[1] == advertiser.addr[1] &&
         scan_resp->address.addr[2] == advertiser.addr[2] &&
         scan_resp->address.addr[3] == advertiser.addr[3] &&
         scan_resp->address.addr[4] == advertiser.addr[4] &&
         scan_resp->address.addr[5] == advertiser.addr[5] &&
         connection == 0) {
          sl_bt_scanner_stop();
          app_log("Found it - %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  advertiser.addr[5],
                  advertiser.addr[4],
                  advertiser.addr[3],
                  advertiser.addr[2],
                  advertiser.addr[1],
                  advertiser.addr[0]);
          sl_bt_connection_open(scan_resp->address, scan_resp->address_type, scan_resp->primary_phy, &connection);
          app_log("Connection initiated - %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                  advertiser.addr[5],
                  advertiser.addr[4],
                  advertiser.addr[3],
                  advertiser.addr[2],
                  advertiser.addr[1],
                  advertiser.addr[0]);
        }
      } break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
