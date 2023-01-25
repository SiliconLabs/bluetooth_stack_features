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
#include "app_log.h"
#include "sl_simple_button_instances.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;


/* External signals used in external signal stack event */
#define BUTTON0_PRESSED 0x00000001
#define BUTTON1_PRESSED 0x00000002

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
      app_log("Boot event - starting advertising\r\n");
      app_log("GATT database version: 1\r\n");

      /* Enable bondings in security manager (this is needed for Service Change Indications) */
      sc = sl_bt_sm_configure( 2, sl_bt_sm_io_capability_noinputnooutput);
      app_assert_status(sc);

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

      // Start general advertising and enable connections.
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 advertiser_general_discoverable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to generate data\n",
                    (int)sc);
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         advertiser_connectable_scannable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("Connection opened\r\n");
      if(evt->data.evt_connection_opened.bonding != 0xFF){
          app_log("Device is already bonded\r\n");
      }
      break;
    case sl_bt_evt_sm_bonded_id:
      app_log("Bonding created\r\n");
      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      app_log("Confirming bonding\r\n");
      sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
    break;

    case sl_bt_evt_sm_bonding_failed_id:
      app_log("bonding failed: %x\r\n", evt->data.evt_sm_bonding_failed.reason);
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log("connection closed, reason: 0x%2.2x\r\n", evt->data.evt_connection_closed.reason);
      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 advertiser_general_discoverable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to generate data\n",
                    (int)sc);
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         advertiser_connectable_scannable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;
    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & BUTTON0_PRESSED) {
        /* if button 0 was pressed, enable the first feature set in the GATT database */
        sc = sl_bt_gatt_server_set_capabilities(Feature1, 0);
        app_assert_status(sc);

        app_log("GATT database version: 1\r\n");
        }
        if (evt->data.evt_system_external_signal.extsignals & BUTTON1_PRESSED) {
        /* if button 1 was pressed, enable the second feature set in the GATT database */
        sc = sl_bt_gatt_server_set_capabilities(Feature2, 0);
        app_assert_status(sc);

        app_log("GATT database version: 2\r\n");
        }
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

/**************************************************************************//**
 * Button press event handler.
 * This overrides the dummy weak implementation.
 *
 * Sets an event to the stack handling function, depending on which button was pressed
 * The event will be handled there, as BGAPI functions (other than the external signal)
 * are not allowed to be called from interrupt context
 *
 * @param[in] handle Handle structure of the button which triggered the event
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  sl_button_state_t btn_state;

  if (SL_SIMPLE_BUTTON_GET_PIN(sl_button_btn0.context) == SL_SIMPLE_BUTTON_GET_PIN(handle->context)){
      btn_state = sl_simple_button_get_state(sl_button_btn0.context);
      if(btn_state == SL_SIMPLE_BUTTON_PRESSED){
          sl_bt_external_signal(BUTTON0_PRESSED);
      }
   }
  else if(SL_SIMPLE_BUTTON_GET_PIN(sl_button_btn1.context) == SL_SIMPLE_BUTTON_GET_PIN(handle->context)){
      btn_state = sl_simple_button_get_state(sl_button_btn1.context);
      if(btn_state == SL_SIMPLE_BUTTON_PRESSED){
          sl_bt_external_signal(BUTTON1_PRESSED);
      }
   }

}
