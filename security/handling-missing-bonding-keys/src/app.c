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

enum SOFT_TIMER_HANDLES
{
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

  app_log("BLE SECURITY TEST APPLICATION : Starting application log... \r\n");
  app_log("App init \r\n");
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

  switch (SL_BT_MSG_ID(evt->header))
  {
  // -------------------------------
  // This event indicates the device has started and the radio is ready.
  // Do not call any stack command before receiving this boot event!
  case sl_bt_evt_system_boot_id:
    app_log("stack version: %u.%u.%u\r\r\n", evt->data.evt_system_boot.major, evt->data.evt_system_boot.minor, evt->data.evt_system_boot.patch);
    // Extract unique ID from BT Address.
    sc = sl_bt_system_get_identity_address(&address, &address_type);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to get Bluetooth address\n",
                  (int)sc);

    app_log("local BT device address: ");
    for (i = 0; i < 5; i++)
    {
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
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to write attribute\r\n",
                  (int)sc);

    //If you don't want to delete the bonding keys when resetting, keep PB0 and PB1 pressed duing reset.
    if (sl_button_btn0.get_state(sl_button_btn0.context) && sl_button_btn1.get_state(sl_button_btn1.context))
    {
      app_log("Clearing already stored bondings !\r\n");
      sc = sl_bt_sm_delete_bondings();
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to delete the bonding keys\r\n",
                    (int)sc);
    }

    sc = sl_bt_sm_store_bonding_configuration(8, 2);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the maximum allowed bonding count and bonding policy\r\n",
                  (int)sc);

    sc = sl_bt_sm_set_passkey(-1);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set using random passkeys\r\n",
                  (int)sc);

    sc = sl_bt_sm_configure(0x0B, sm_io_capability_displayonly);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to configure security requirements and I/O capabilities of the system\r\n",
                  (int)sc);

    sc = sl_bt_sm_set_bondable_mode(1);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the device accept new bondings\r\n",
                  (int)sc);

    // Create an advertising set.
    sc = sl_bt_advertiser_create_set(&advertising_set_handle);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to create advertising set\r\n",
                  (int)sc);

    sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        1216, // min. adv. interval
        2056, // max. adv. interval
        0,    // adv. duration
        0);   // max. num. adv. events
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set advertising timing\r\n",
                  (int)sc);
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

  case sl_bt_evt_system_soft_timer_id:
    switch (evt->data.evt_system_soft_timer.handle)
    {
    case INCREASE_SEC_TMR_HANDLE:
      app_log("Increasing security after delay...\r\n");
      sc = sl_bt_sm_increase_security(connectionToIncreasSec);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to enhance the security\r\n",
                    (int)sc);
      break;
    default:
      break;
    }
    break;

  // -------------------------------
  // This event indicates that a new connection was opened.
  case sl_bt_evt_connection_opened_id:
    app_log("Bluetooth Stack Event : CONNECTION OPENED\r\n");
    sc = sl_bt_advertiser_stop(advertising_set_handle);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to stop the advertising\r\n",
                  (int)sc);

    // Store the connection ID
    uint8_t activeConnectionId = 0;
    activeConnectionId = evt->data.evt_connection_opened.connection;
    bleBondingHandle = evt->data.evt_connection_opened.bonding;

    if (bleBondingHandle == 0xFF)
    {
      app_log("Increasing security\r\n");
      sc = sl_bt_sm_increase_security(activeConnectionId);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to enhance the security\r\n",
                    (int)sc);
    }
    else
    {
      app_log("Already Bonded (ID: %d)\r\n", bleBondingHandle);
    }
    break;

  case sl_bt_evt_connection_parameters_id:
  {
    app_log("Bluetooth Stack Event : CONNECTION Parameters ID\r\n");
    uint8_t connectionhandle = evt->data.evt_connection_parameters.connection;
    uint16_t txSize = evt->data.evt_connection_parameters.txsize;
    uint8_t securityLevel = evt->data.evt_connection_parameters.security_mode + 1;
    uint16_t timeout = evt->data.evt_connection_parameters.timeout;
    if (securityLevel > 2)
    {
      app_log("[OK]      Bluetooth Stack Event : CONNECTION PARAMETERS : MTU = %d, SecLvl : %d, Timeout : %d\r\n", txSize, securityLevel, timeout);
    }
    else
    {
      app_log("Bluetooth Stack Event : CONNECTION PARAMETERS : MTU = %d, SecLvl : %d, timeout : %d\r\n", txSize, securityLevel, timeout);
    }

    //If security is less than 2 increase so devices can bond
    if (securityLevel < 2)
    {
      app_log("Bonding Handle is: 0x%04X\r\n", bleBondingHandle);
      if (bleBondingHandle == 0xFF)
      {
        app_log("Increasing security.\r\n");
        sc = sl_bt_sm_increase_security(connectionhandle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to enhance the security\r\n",
                      (int)sc);

        connectionToIncreasSec = connectionhandle;
      }
      else
      {
        app_log("Increasing security..\r\n");
        sc = sl_bt_sm_increase_security(connectionhandle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to enhance the security\r\n",
                      (int)sc);
      }
    }
    else
    {
      app_log("Clear increase sec timer.\r\n");
      sl_bt_system_set_soft_timer(0, INCREASE_SEC_TMR_HANDLE, 1); //Clear timer
    }
    break;
  }

  // -------------------------------
  // This event indicates that a connection was closed.
  case sl_bt_evt_connection_closed_id:
    bleBondingHandle = 0xFF; //reset bonding handle variable to avoid deleting wrong bonding info
    app_log("Bluetooth Stack Event : CONNECTION CLOSED (reason: 0x%04X)\r\n", evt->data.evt_connection_closed.reason);

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

  case sl_bt_evt_sm_passkey_display_id:
    app_log("Passkey %06ld\r\n", evt->data.evt_sm_passkey_display.passkey);
    app_log("Bluetooth Stack Event : PASSKEY DISPLAY\r\n");
    break;

  case sl_bt_evt_sm_confirm_passkey_id:
    app_log("Bluetooth Stack Event : PASSKEY CONFIRM\r\n");
    break;

  case sl_bt_evt_sm_passkey_request_id:
    app_log("Bluetooth Stack Event : PASSKEY REQUEST\r\n");
    break;

  case sl_bt_evt_sm_confirm_bonding_id:
    app_log("Bluetooth Stack Event : CONFIRM BONDING\r\n");
    uint8_t connectionhandle = evt->data.evt_connection_parameters.connection;

    /* This command automatically confirms bonding requests. This is not
      recommended in real life applications for security purposes.
      Ideally you want the user to accept the bonding request*/
    sc = sl_bt_sm_bonding_confirm(connectionhandle, 1);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to confirm bonding requests\r\n",
                  (int)sc);
    break;

  case sl_bt_evt_sm_bonded_id:
    app_log("Bluetooth Stack Event : BONDED\r\n");
    break;

  case sl_bt_evt_sm_bonding_failed_id:
  {
    uint8_t connectionHandle = evt->data.evt_sm_bonding_failed.connection;
    uint16_t reason = evt->data.evt_sm_bonding_failed.reason;
    app_log("Bluetooth Stack Event : BONDING FAILED (connection: %d, reason: 0x%04X, bondingHandle: %d)\r\n",
               connectionHandle, reason, bleBondingHandle);
    if (reason == SL_STATUS_BT_SMP_PASSKEY_ENTRY_FAILED || reason == SL_STATUS_TIMEOUT)
    {
      app_log("Increasing security... because reason is 0x%04x\r\n", reason);
      sc = sl_bt_sm_increase_security(connectionHandle);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to enhance the security\r\n",
                    (int)sc);
    }

    if (reason == SL_STATUS_BT_SMP_PAIRING_NOT_SUPPORTED || reason == SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING)
    {
      if (bleBondingHandle != 0xFF)
      {
        app_log("Broken bond, deleting ID:%d...\r\n", bleBondingHandle);
        sc = sl_bt_sm_delete_bonding(bleBondingHandle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to delete specified bonding information or whitelist\r\n",
                      (int)sc);

        sc = sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to enhance the security\r\n",
                      (int)sc);

        bleBondingHandle = 0xFF;
      }
      else
      {
        app_log("Increasing security in one second...\r\n");
        uint16_t result;
        result = sl_bt_sm_increase_security(connectionHandle);
        app_log("Result... = 0x%04X\r\n", result);
        app_assert(result == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to enhance the security\r\n",
                      (int)sc);

        if (result == SL_STATUS_INVALID_STATE)
        {
          app_log("Trying to increase security again");
          sc = sl_bt_sm_increase_security(connectionHandle);
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to enhance the security\r\n",
                        (int)sc);
        }
      }
    }

    if (reason == SL_STATUS_BT_SMP_UNSPECIFIED_REASON)
    {
      app_log("Increasing security... because reason is 0x0308\r\n");
      sl_bt_sm_increase_security(connectionHandle);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to enhance the security\r\n",
                    (int)sc);
    }
    break;
  }

  case sl_bt_evt_gatt_server_execute_write_completed_id:
    app_log("Bluetooth Stack Event : GATT SERVER WRITE COMPLETED\r\n");
    break;

  case sl_bt_evt_gatt_server_characteristic_status_id:
    app_log("Bluetooth Stack Event : GATT SERVER CHARACTERISTIC STATUS\r\n");
    switch (evt->data.evt_gatt_server_characteristic_status.characteristic)
    {
    default:
      app_log("GATT unknown...\r\n");
      break;
    }
    break;

  case sl_bt_evt_gatt_server_attribute_value_id:
    app_log("Bluetooth Stack Event : GATT SERVER ATTRIBUTE VALUE\r\n");
    switch (evt->data.evt_gatt_server_attribute_value.attribute)
    {
    default:
      app_log("unkown attribute???\r\n");
      break;
    }
    break;

  case sl_bt_evt_system_external_signal_id:
    app_log("Bluetooth Stack Event : EXTERNAL SIGNAL\r\n");
    break;

  case sl_bt_evt_gatt_server_user_write_request_id:
    app_log("Bluetooth Stack Event : GATT SERVER USER WRITE REQUEST\r\n");
    break;

  ///////////////////////////////////////////////////////////////////////////
  // Add additional event handlers here as your application requires!      //
  ///////////////////////////////////////////////////////////////////////////

  // -------------------------------
  // Default event handler.
  default:
    app_log("Bluetooth Stack Event : UNKNOWN (0x%04lx)\r\n", SL_BT_MSG_ID(evt->header));
    break;
  }
}
