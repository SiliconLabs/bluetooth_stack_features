/***************************************************************************/ /**
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
#include "sl_simple_button_instances.h"
#include "app_log.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

uint8_t bleBondingHandle = 0xFF;

#define BSP_LED_OFF (1)
#define BSP_LED_ON  (0)

uint8_t connectionToIncreasSec = 0xFF;

enum SOFT_TIMER_HANDLES{
  INCREASE_SEC_TMR_HANDLE = 1,
};
/**************************************************************************/ /**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////

  app_log_info("BLE SECURITY TEST APPLICATION : Starting application log... \r\n");
  app_log_info("App init \r\n");
}

/**************************************************************************/ /**
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

/**************************************************************************/ /**
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
  uint8_t i;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      app_log_info("stack version: %u.%u.%u\r\r\n", evt->data.evt_system_boot.major, evt->data.evt_system_boot.minor, evt->data.evt_system_boot.patch);
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      app_log_info("local BT device address: ");
      for (i = 0; i < 5; i++) {
        app_log("%2.2x:", address.addr[5 - i]);
      }
      app_log("%2.2x\r\r\n", address.addr[0]);
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

      //If you want to delete the bonding keys when resetting, keep PB0  pressed duing reset.
      if (sl_button_btn0.get_state(&sl_button_btn0)) {
        app_log_info("Clearing already stored bondings !\r\n");
        sc = sl_bt_sm_delete_bondings();
        app_assert_status(sc);
      }

      sc = sl_bt_sm_store_bonding_configuration(8, 2);
      app_assert_status(sc);

      sc = sl_bt_sm_set_passkey(-1);
      app_assert_status(sc);

      sc = sl_bt_sm_configure(0x0B, sl_bt_sm_io_capability_displayonly);
      app_assert_status(sc);

      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        1216, // min. adv. interval
        2056, // max. adv. interval
        0,    // adv. duration
        0);   // max. num. adv. events
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      // Start general advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("Bluetooth Stack Event : CONNECTION OPENED\r\n");
      sc = sl_bt_advertiser_stop(advertising_set_handle);
      app_assert_status(sc);

      // Store the connection ID
      uint8_t activeConnectionId = 0;
      activeConnectionId = evt->data.evt_connection_opened.connection;
      bleBondingHandle = evt->data.evt_connection_opened.bonding;
      // a delay could be needed before sending the security request to the phone, sending the request too soon
      // can cause the phone to drop it. 0.5s is a suggested value
      sl_sleeptimer_delay_millisecond(500);
      if (bleBondingHandle == 0xFF) {
        app_log_info("Increasing security\r\n");
        sc = sl_bt_sm_increase_security(activeConnectionId);
        app_assert_status(sc);
      } else {
        app_log_info("Already Bonded (ID: %d)\r\n", bleBondingHandle);
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      bleBondingHandle = 0xFF; //reset bonding handle variable to avoid deleting wrong bonding info
      app_log_info("Bluetooth Stack Event : CONNECTION CLOSED (reason: 0x%04X)\r\n", evt->data.evt_connection_closed.reason);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_passkey_display_id:
      app_log_info("Passkey %06ld\r\n", evt->data.evt_sm_passkey_display.passkey);
      app_log_info("Bluetooth Stack Event : PASSKEY DISPLAY\r\n");
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      app_log_info("Bluetooth Stack Event : PASSKEY CONFIRM\r\n");
      break;

    case sl_bt_evt_sm_passkey_request_id:
      app_log_info("Bluetooth Stack Event : PASSKEY REQUEST\r\n");
      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      app_log_info("Bluetooth Stack Event : CONFIRM BONDING\r\n");
      uint8_t connectionhandle = evt->data.evt_connection_parameters.connection;

      /* This command automatically confirms bonding requests. This is not
         recommended in real life applications for security purposes.
         Ideally you want the user to accept the bonding request*/
      sc = sl_bt_sm_bonding_confirm(connectionhandle, 1);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log_info("Bluetooth Stack Event : BONDED\r\n");
      break;

    case sl_bt_evt_sm_bonding_failed_id:
    {
      uint8_t connectionHandle = evt->data.evt_sm_bonding_failed.connection;
      uint16_t reason = evt->data.evt_sm_bonding_failed.reason;
      app_log_info("Bluetooth Stack Event : BONDING FAILED (connection: %d, reason: 0x%04X, bondingHandle: %d)\r\n",
                   connectionHandle, reason, bleBondingHandle);

      if (reason == SL_STATUS_BT_SMP_PASSKEY_ENTRY_FAILED
          || reason == SL_STATUS_TIMEOUT || reason == SL_STATUS_BT_SMP_UNSPECIFIED_REASON) {
        app_log_info("Increasing security... reason: 0x%04x\r\n", reason);
        sc = sl_bt_sm_increase_security(connectionHandle);
        app_assert_status(sc);
      } else if (reason == SL_STATUS_BT_SMP_PAIRING_NOT_SUPPORTED
                 || reason == SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING) {
        if (bleBondingHandle != 0xFF) {
          app_log_info("Broken bond, deleting ID:%d...\r\n", bleBondingHandle);
          sc = sl_bt_sm_delete_bonding(bleBondingHandle);
          app_assert_status(sc);
          bleBondingHandle = 0xFF;
        }
        sc = sl_bt_sm_increase_security(connectionHandle);
        app_log_info("Result... = 0x%08lX\r\n", sc);
        app_assert_status(sc);
      }
    }
    break;

    case sl_bt_evt_gatt_server_execute_write_completed_id:
      app_log_info("Bluetooth Stack Event : GATT SERVER WRITE COMPLETED\r\n");
      break;

    case sl_bt_evt_gatt_server_characteristic_status_id:
      app_log_info("Bluetooth Stack Event : GATT SERVER CHARACTERISTIC STATUS\r\n");
      switch (evt->data.evt_gatt_server_characteristic_status.characteristic) {
        default:
          app_log_info("GATT unknown...\r\n");
          break;
      }
      break;

    case sl_bt_evt_gatt_server_attribute_value_id:
      app_log_info("Bluetooth Stack Event : GATT SERVER ATTRIBUTE VALUE\r\n");
      switch (evt->data.evt_gatt_server_attribute_value.attribute) {
        default:
          app_log_info("unknown attribute???\r\n");
          break;
      }
      break;

    case sl_bt_evt_system_external_signal_id:
      app_log_info("Bluetooth Stack Event : EXTERNAL SIGNAL\r\n");
      break;

    case sl_bt_evt_gatt_server_user_write_request_id:
      app_log_info("Bluetooth Stack Event : GATT SERVER USER WRITE REQUEST\r\n");
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
//    for Debug Purposes
//    app_log_info("Bluetooth Stack Event : UNKNOWN (0x%04lx)\r\n", SL_BT_MSG_ID(evt->header));
      break;
  }
}
