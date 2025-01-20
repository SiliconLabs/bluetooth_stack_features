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
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "em_gpio.h"
#include "gpiointerrupt.h"
#include "custom_adv.h"
#include "sl_simple_button_btn0_config.h"
#include "sl_simple_button_btn1_config.h"
#include "app_log.h"
#include "sl_simple_button_instances.h"


CustomAdv_t sData; // Our custom advertising data stored here

uint8_t num_presses = 0;
uint8_t last_press = 0xFF;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static void handle_button_press(int button)
{
  if (button > 0) {
    num_presses++;
  } else {
    if (num_presses > 0) num_presses--;
  }
  last_press = button;
  // Update the advertising data on-the-fly
  update_adv_data(&sData, advertising_set_handle, num_presses, last_press);
}

/******************************************
 * Callback when button state change
 ******************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if((handle == &sl_button_btn0) &&
      (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)) { //if button 0 is pressed
	  sl_bt_external_signal(0x1);
  } else if ((handle == &sl_button_btn1) &&
      (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED)) { //if button 1 is pressed
		sl_bt_external_signal(0x2); 
	  }
}

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

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert(sc == SL_STATUS_OK,
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
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to write attribute\n",
                    (int)sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to create advertising set\n",
                    (int)sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      sl_bt_advertiser_set_channel_map(advertising_set_handle, 7);

      fill_adv_packet(&sData, 0x06, 0x02FF, num_presses, last_press, "CustomAdvDemo");
      start_adv(&sData, advertising_set_handle);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising timing\n",
                    (int)sc);
      app_log("Start advertising ...\n");
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable );
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to generate data\n",
                    (int)sc);
      sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_legacy_advertiser_connectable);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & 0x1) {
          handle_button_press(0);
      }
      else if (evt->data.evt_system_external_signal.extsignals & 0x2) {
          handle_button_press(1);
      }
      break;

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
