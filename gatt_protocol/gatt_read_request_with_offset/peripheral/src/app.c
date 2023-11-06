/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "app.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

#define PAYLOAD_SIZE  330
#define MTU_SIZE  204

uint8_t payload[PAYLOAD_SIZE];
uint16_t mtu_size;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  app_log("Boot\r\n");

  for (uint16_t i = 0; i < PAYLOAD_SIZE; i++) {
    payload[i] = 0xAA;
  }
  payload[0] = 0xBB;
  payload[197] = 0xCC;

  payload[196] = 0xDD;
  payload[132] = 0xEE;

  payload[329] = 0xFF;
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_gatt_mtu_exchanged_id:
      app_log("Exchanged Mtu: %d\r\n", evt->data.evt_gatt_mtu_exchanged.mtu);
      sc = sl_bt_gatt_server_set_max_mtu(MTU_SIZE, &mtu_size);
      app_assert_status(sc);

      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    case sl_bt_evt_gatt_server_user_read_request_id:
      app_log("Data ");
      //app_log("MtuSize %d\r\n", mtu_size);

      uint16_t mtu;
      uint16_t sent_len;
      sc = sl_bt_gatt_server_get_mtu(evt->data.evt_gatt_server_user_read_request.connection, &mtu);
      uint16_t offset = evt->data.evt_gatt_server_user_read_request.offset;

      //app_log("Mtu %d\r\n", mtu);
      //app_log("Offset %d\r\n", offset);

      mtu = mtu_size - 4; // ATT_READ_BY_TYPE_REQ 4 bytes overhead
      uint16_t raw_size = PAYLOAD_SIZE - offset;
      raw_size = raw_size > mtu ? mtu : raw_size;
      sc = sl_bt_gatt_server_send_user_read_response(evt->data.evt_gatt_server_user_read_request.connection,
                                                     evt->data.evt_gatt_server_user_read_request.characteristic,
                                                     SL_STATUS_OK, raw_size, &payload[offset], &sent_len);
      app_assert_status(sc);
      //app_log("SentLen %d\r\n", sent_len);
      app_log("Sent\r\n");
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.

    default:
      app_log_debug("Unhandled event [0x%08lx]\r\n",
                    SL_BT_MSG_ID(evt->header));
      break;
  }
}
