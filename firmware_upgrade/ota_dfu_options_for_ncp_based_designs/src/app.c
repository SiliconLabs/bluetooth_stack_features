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
#include <stdio.h>
#include "em_common.h"
#include "sl_ncp.h"
#include "app.h"
#include "gatt_db.h"

#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"

#include "btl_interface.h"
#include "btl_interface_storage.h"

// Use APP_VERSION to build two slightly different variants of the same example
#define APP_VERSION 1


#define DEBUG_MESSAGE_BTN_PRESS          0x01

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

/***************************************************************************//**
 * Application Init.
 ******************************************************************************/
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

/***************************************************************************//**
 * User command (message_to_target) handler callback.
 *
 * Handles user defined commands received from NCP host.
 * The user commands handled here are defined in app.h and are solely meant for
 * example purposes.
 * @param[in] data Data received from NCP host.
 *
 * @note This overrides the dummy weak implementation.
 ******************************************************************************/
void sl_ncp_user_cmd_message_to_target_cb(void *data)
{
  uint8array *cmd = (uint8array *)data;
  user_cmd_t *user_cmd = (user_cmd_t *)cmd->data;

  switch (user_cmd->hdr) {
    // -------------------------------
    // Example: user command 1.
    case USER_CMD_1_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////

      // Send response to user command 1 to NCP host.
      // Example: sending back received command.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_OK, cmd->len, cmd->data);
      break;

    // -------------------------------
    // Example: user command 2.
    case USER_CMD_2_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////

      // Send response to user command 2 to NCP host.
      // Example: sending back received command.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_OK, cmd->len, cmd->data);
      // Send user event too.
      // Example: sending back received command as an event.
      sl_ncp_user_evt_message_to_target(cmd->len, cmd->data);
      break;

    // -------------------------------
    // Unknown user command.
    default:
      // Send error response to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_FAIL, 0, NULL);
      break;
  }
}

/**************************************************************************//**
 * Local event processor.
 *
 * Use this function to process Bluetooth stack events locally on NCP.
 * Set the return value to true, if the event should be forwarded to the
 * NCP-host. Otherwise, the event is discarded locally.
 * Implement your own function to override this default weak implementation.
 *
 * @param[in] evt The event.
 *
 * @return true, if we shall send the event to the host. This is the default.
 *
 * @note Weak implementation.
 *****************************************************************************/
bool sl_ncp_local_evt_process(sl_bt_msg_t *evt)
{
  bool evt_handled = true;
  /* OTA variables */
  static uint32_t ota_image_position = 0;
  static uint8_t ota_in_progress = 0;
  static uint8_t ota_image_finished=0;
  static int32_t verify_result = -1;
  uint8_t debug_message;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    case sl_bt_evt_system_boot_id:
      {
        size_t name_len;
        char name[10];
        char slot_erased = ' '; // this character is appended to device name to indicate if slot is erased or not

        // erase download slot if button PB0 is pressed at reboot
        if (sl_button_get_state(&sl_button_btn1) == SL_SIMPLE_BUTTON_PRESSED) {
            debug_message = DEBUG_MESSAGE_BTN_PRESS;
            sl_bt_send_evt_user_message_to_host((uint8_t) sizeof(debug_message),
                                                &debug_message);
            sl_led_turn_on(&sl_led_led0);
            bootloader_eraseStorageSlot(0);
            sl_bt_system_set_soft_timer(32768, 10, 0);
            slot_erased = '*';
        }

        sprintf(name, "NCP APP %d%c", (int)APP_VERSION, slot_erased);

        // set device name:
        name_len = strlen(name);
        sl_bt_gatt_server_write_attribute_value(gattdb_device_name,
                                                0,
                                                name_len,
                                                (uint8_t*)name);

        // Start advertising automatically at boot (just to make testing easier)
        // Create an advertising set.
        sl_bt_advertiser_create_set(&advertising_set_handle);

        // Start advertising
        sl_bt_advertiser_start(advertising_set_handle,
                               advertiser_general_discoverable,
                               advertiser_connectable_scannable);
      }
      break;

    case sl_bt_evt_system_soft_timer_id:
      {
        if (evt->data.evt_system_soft_timer.handle == 10) {
            sl_led_toggle(&sl_led_led0);
        }

        // not pass to host
        evt_handled = true;
      }
      break;

    case sl_bt_evt_gatt_server_user_write_request_id:
      {
        int32_t err = BOOTLOADER_OK;
        // OTA support
        uint32_t connection = evt->data.evt_gatt_server_user_write_request.connection;
        uint32_t characteristic = evt->data.evt_gatt_server_user_write_request.characteristic;

        if (characteristic == gattdb_ota_control) {
          switch (evt->data.evt_gatt_server_user_write_request.value.data[0]) {
            case 0:
              // Initialize OTA update (note: erase of slot is already performed!)
              bootloader_init();
              ota_image_position=0;
              ota_in_progress=1;
              break;
            case 3://END OTA process
              // Wait for connection close and then reboot
              ota_in_progress=0;
              ota_image_finished=1;

              err = bootloader_verifyImage(0, NULL);
              verify_result = err; // make copy of the result for later use
              break;
            default:
              break;
          }
          evt_handled = true;
        } else if (characteristic == gattdb_ota_data) {
            if (ota_in_progress) {
              bootloader_writeStorage(0,//use slot 0
                                      ota_image_position,
                                      evt->data.evt_gatt_server_user_write_request.value.data,
                                      evt->data.evt_gatt_server_user_write_request.value.len);

              ota_image_position+=evt->data.evt_gatt_server_user_write_request.value.len;
            }
            evt_handled = true;
        }

        if (evt_handled == true) {
          if (err != BOOTLOADER_OK) {
              sl_bt_gatt_server_send_user_write_response(connection,
                                                         characteristic,
                                                         err);
          } else {
              sl_bt_gatt_server_send_user_write_response(connection,
                                                         characteristic,
                                                         err);
          }
        }
      }
      break;

    case sl_bt_evt_connection_closed_id:
        if (ota_image_finished) {
          if (verify_result == BOOTLOADER_OK) {
            bootloader_setImageToBootload(0);
            bootloader_rebootAndInstall();
          } else {
            uint8_t name_len;
            char name[10];
            // Something went wrong: restart advertising and indicate the error code in device name
            sprintf(name, "err %lx", verify_result);
            name_len = strlen(name);
            sl_bt_gatt_server_write_attribute_value(gattdb_device_name,
                                                    0,
                                                    name_len,
                                                    (uint8_t*)name);
            sl_bt_advertiser_start(advertising_set_handle,
                                   advertiser_general_discoverable,
                                   advertiser_connectable_scannable);
          }
        } else {
            /* Restart advertising after client has disconnected */
            sl_bt_advertiser_start(advertising_set_handle,
                                   advertiser_general_discoverable,
                                   advertiser_connectable_scannable);
        }
      break;

    default:
      break;
  }

  return evt_handled;
}
