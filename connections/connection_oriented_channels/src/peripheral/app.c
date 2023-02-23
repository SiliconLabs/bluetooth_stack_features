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
#include "gatt_db.h"
#include "app.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"


#define SIGNAL_BTN_PRESS  1

#define INITIAL_CREDITS         50
#define CREDITS_PER_BUTTONPRESS 3

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t connection;
static uint16_t cid, max_pdu, max_sdu, credit = INITIAL_CREDITS;
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      app_log("CoC sample application, role: peripheral\r\n");
      sl_simple_led_turn_on(sl_led_led0.context);

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

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection = evt->data.evt_connection_opened.connection;
      app_log("Connection opened\r\n");
      break;

    case sl_bt_evt_l2cap_le_channel_open_request_id:
      app_log("L2CAP channel open request received\r\n");
      max_pdu = evt->data.evt_l2cap_le_channel_open_request.max_pdu;
      max_sdu = evt->data.evt_l2cap_le_channel_open_request.max_sdu;
      cid = evt->data.evt_l2cap_le_channel_open_request.cid;
      sc = sl_bt_l2cap_send_le_channel_open_response(connection, cid,
                                          max_sdu, max_pdu, INITIAL_CREDITS, 0);
      app_assert_status(sc);
      break;

    case sl_bt_evt_l2cap_channel_data_id:
      app_log("Data packet received\r\n");
      break;

    case sl_bt_evt_system_external_signal_id:
          if(evt->data.evt_system_external_signal.extsignals == SIGNAL_BTN_PRESS){
             sl_bt_l2cap_channel_send_credit(connection, cid, CREDITS_PER_BUTTONPRESS);
             credit += CREDITS_PER_BUTTONPRESS;
            }
          break;

    case sl_bt_evt_l2cap_channel_closed_id:
       app_log("L2CAP channel closed, reason: %x\r\n", evt->data.evt_l2cap_channel_closed.reason);
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
      break;
  }
}

void sl_button_on_change(const sl_button_t *handle)
{
  sl_button_state_t state;
  if(handle->context == sl_button_btn0.context){
      state = sl_button_get_state(&sl_button_btn0);
      if(state == SL_SIMPLE_BUTTON_PRESSED){
          sl_bt_external_signal(SIGNAL_BTN_PRESS);
      }
  }
}

