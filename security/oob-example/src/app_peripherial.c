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
#include "sl_iostream_init_usart_instances.h"
#include "app_log.h"

#define LEGACY_PAIRING            1
#define SECURE_CONNECTION_PAIRING 2

#define PAIRING_MODE SECURE_CONNECTION_PAIRING

#define NOTIFY_TIMER    1

#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 4
#endif

#define STATE_IDLE_MODE 0
#define STATE_OOB_MODE  1

static void read_oob_data();

uint8_t rx_oob_buf[32];
static uint16_t notifyEnabled = 0;
static uint8_t connHandle = 0xFF;
static uint8_t notifyBuf[20] = {0};
// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t _oob_state = STATE_IDLE_MODE;
/**************************************************************************/ /**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
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
  if (STATE_OOB_MODE == _oob_state)
  {
    read_oob_data();
  }
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

  switch (SL_BT_MSG_ID(evt->header))
  {
  // -------------------------------
  // This event indicates the device has started and the radio is ready.
  // Do not call any stack command before receiving this boot event!
  case sl_bt_evt_system_boot_id:
    memset(notifyBuf, '0', 20);
    app_log("stack version: %u.%u.%u\r\r\n", evt->data.evt_system_boot.major, evt->data.evt_system_boot.minor, evt->data.evt_system_boot.patch);
    app_log("Peripheral Boot\r\n");
    // Extract unique ID from BT Address.
    sc = sl_bt_system_get_identity_address(&address, &address_type);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to get Bluetooth address\n",
                  (int)sc);
    app_log("local BT device address: ");
    for (uint8_t i = 0; i < 5; i++)
    {
      app_log("%2.2x:", address.addr[5 - i]);
    }
    app_log("%2.2x\r\n", address.addr[0]);
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
    app_log("All bonding deleted\r\n");
    sc = sl_bt_sm_delete_bondings();
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to delete bondings\r\n",
                  (int)sc);

    /* bit 3 of flag is 0 to allow legacy pairing */
    sc = sl_bt_sm_configure(0x0B, sm_io_capability_keyboarddisplay);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to configure security requirements and I/O capabilities of the system\r\n",
                  (int)sc);

    sc = sl_bt_sm_set_bondable_mode(1);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the device accept new bondings\r\n",
                  (int)sc);

#if (PAIRING_MODE == SECURE_CONNECTION_PAIRING)
    app_log("Use Secure Connection Pairing.\r\n");
    app_log("Enter 32-byte OOB data and confirm value.\r\n");
    sc = sl_bt_sm_use_sc_oob(1, 32, NULL, NULL);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to enable the use of OOB data and confirm value\r\n",
                  (int)sc);
#elif (PAIRING_MODE == LEGACY_PAIRING)
    app_log("Use Legacy Pairing.\r\n");
    app_log("Enter 16-byte OOB data.\r\n");
#endif
    _oob_state = STATE_OOB_MODE;
    break;

  // -------------------------------
  // This event indicates that a new connection was opened.
  case sl_bt_evt_connection_opened_id:
    app_log("Connected\r\n");
    connHandle = evt->data.evt_connection_opened.connection;

    sc = sl_bt_sm_increase_security(connHandle);
    app_assert(sc == SL_STATUS_OK,
                  "Increasing security error, error code = 0x%04x\r\n",
                  (int)sc);
    app_log("Increasing security\r\n");
    break;

  // -------------------------------
  // This event indicates that a connection was closed.
  case sl_bt_evt_connection_closed_id:
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
  case sl_bt_evt_gatt_server_characteristic_status_id:
    /* Char status changed */
    if ((evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_ntf_char) && (evt->data.evt_gatt_server_characteristic_status.status_flags == 0x01))
    {
      notifyEnabled = evt->data.evt_gatt_server_characteristic_status.client_config_flags;
    }
    if (notifyEnabled == gatt_notification)
    {
      sc = sl_bt_system_set_soft_timer(32768 * 3, NOTIFY_TIMER, false);
    }
    else
    {
      sc = sl_bt_system_set_soft_timer(0, NOTIFY_TIMER, true);
    }
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set soft timer\n",
                  (int)sc);
    break;

  case sl_bt_evt_gatt_server_attribute_value_id:
    if ((evt->data.evt_gatt_server_attribute_value.att_opcode == gatt_write_request) && (evt->data.evt_gatt_server_attribute_value.attribute == gattdb_wrt_char))
    {
      app_log("Gatt Write Received: ");
      for (uint8_t i = 0; i < evt->data.evt_gatt_server_attribute_value.value.len; i++)
      {
        app_log("%c", evt->data.evt_gatt_server_attribute_value.value.data[i]);
      }
      app_log("\r\n");
    }
    break;

  case sl_bt_evt_system_soft_timer_id:
    /* Check which software timer handle is in question */
    switch (evt->data.evt_system_soft_timer.handle)
    {
    case NOTIFY_TIMER:
      if ((connHandle != 0xFF) && (notifyEnabled == gatt_notification))
      {
        uint8_t i = notifyBuf[0];
        if (i == '9')
        {
          i = '0';
          memset(notifyBuf, i, 20);
        }
        else
        {
          memset(notifyBuf, i + 1, 20);
        }
        sc = sl_bt_gatt_server_send_notification(connHandle, gattdb_ntf_char, 20, notifyBuf);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to send notification\n",
                      (int)sc);
      }
      break;

    default:
      break;
    }
    break;

  case sl_bt_evt_sm_confirm_bonding_id:
    sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
    app_log("Bonding confirmed\r\n");
    break;

  case sl_bt_evt_sm_passkey_display_id:
    app_log("Passkey display\r\n");
    //      app_log("Enter this passkey on the tablet:\r\n%d\r\n",
    //          evt->data.evt_sm_passkey_display.passkey);
    break;

  case sl_bt_evt_sm_passkey_request_id:
    app_log("Passkey request\r\n");
    //      app_log("Enter the passkey you see on the tablet\r\n");
    break;

  case sl_bt_evt_sm_confirm_passkey_id:
    app_log("Confirm passkey\r\n");
    //      app_log("Are you see the same passkey on the tablet: %d (y/n)?\r\n",evt->data.evt_sm_confirm_passkey.passkey);
    break;

  case sl_bt_evt_sm_bonded_id:
    /* The bonding/pairing was successful so set the flag to allow indications to proceed */
    app_log("Bonding completed\r\n");
    break;

  case sl_bt_evt_sm_bonding_failed_id:
    /* If the attempt at bonding/pairing failed, clear the bonded flag and display the reason */
    app_log("Bonding failed: ");
    switch (evt->data.evt_sm_bonding_failed.reason)
    {
    case SL_STATUS_BT_SMP_PASSKEY_ENTRY_FAILED:
      app_log("The user input of passkey failed\r\n");
      break;

    case SL_STATUS_BT_SMP_OOB_NOT_AVAILABLE:
      app_log("Out of Band data is not available for authentication\r\n");
      break;

    case SL_STATUS_BT_SMP_AUTHENTICATION_REQUIREMENTS:
      app_log("The pairing procedure cannot be performed as authentication requirements cannot be met due to IO capabilities of one or both devices\r\n");
      break;

    case SL_STATUS_BT_SMP_CONFIRM_VALUE_FAILED:
      app_log("The confirm value does not match the calculated compare value\r\n");
      break;

    case SL_STATUS_BT_SMP_PAIRING_NOT_SUPPORTED:
      app_log("Pairing is not supported by the device\r\n");
      break;

    case SL_STATUS_BT_SMP_ENCRYPTION_KEY_SIZE:
      app_log("The resultant encryption key size is insufficient for the security requirements of this device\r\n");
      break;

    case SL_STATUS_BT_SMP_COMMAND_NOT_SUPPORTED:
      app_log("The SMP command received is not supported on this device\r\n");
      break;

    case SL_STATUS_BT_SMP_UNSPECIFIED_REASON:
      app_log("Pairing failed due to an unspecified reason\r\n");
      break;

    case SL_STATUS_BT_SMP_REPEATED_ATTEMPTS:
      app_log("Pairing or authentication procedure is disallowed because too little time has elapsed since last pairing request or security request\r\n");
      break;

    case SL_STATUS_BT_SMP_INVALID_PARAMETERS:
      app_log("Invalid Parameters\r\n");
      break;

    case SL_STATUS_BT_SMP_DHKEY_CHECK_FAILED:
      app_log("The bonding does not exist\r\n");
      break;

    case SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING:
      app_log("Pairing failed because of missing PIN, or authentication failed because of missing Key\r\n");
      break;

    case SL_STATUS_TIMEOUT:
      app_log("Operation timed out\r\n");
      break;

    default:
      app_log("Unknown error: 0x%X\r\n", evt->data.evt_sm_bonding_failed.reason);
      break;
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

static void read_oob_data()
{
  sl_status_t sc;
  uint8_t input;
  sc = sl_iostream_getchar(sl_iostream_vcom_handle,
                           (char *)&input);
  static uint8_t num = 0;
  if (sc == SL_STATUS_OK)
  {
#ifdef LOCAL_ECHO
    if (input != '\n')
    {
      app_log("%c", input);
    }
#else
    rx_oob_buf[num++] = input;
#endif
  }
#if (PAIRING_MODE == SECURE_CONNECTION_PAIRING)
  if (num == 32)
  {
    app_log("16-byte OOB data: ");
    for (uint8_t x = 0; x < 16; x++)
    {
      app_log("0x%02X ", rx_oob_buf[x]);
    }
    app_log("\r\n");
    app_log("16-byte confirm value: ");
    for (uint8_t x = 16; x < 32; x++)
    {
      app_log("0x%02X ", rx_oob_buf[x]);
    }
    app_log("\r\n");

    sl_bt_sm_set_sc_remote_oob_data(32, rx_oob_buf);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set OOB data\n",
                  (int)sc);
    /* Set advertising parameters. 100ms advertisement interval. All channels used.
     * The first two parameters are minimum and maximum advertising interval, both in
     * units of (milliseconds * 1.6). The third parameter '7' sets advertising on all channels. */
    sl_bt_advertiser_set_timing(advertising_set_handle, 160, 160, 0, 0);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set advertising parameters\n",
                  (int)sc);

    sc = sl_bt_advertiser_set_channel_map(advertising_set_handle, 7);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the primary advertising channel map\n",
                  (int)sc);
    /* Start general advertising and enable connections. */

    sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                               sl_bt_advertiser_general_discoverable);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to generate data\n",
                  (int)sc);
    sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                       sl_bt_advertiser_connectable_scannable);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start advertising\n",
                  (int)sc);

    num = 0;
    _oob_state = STATE_IDLE_MODE;
  }
#elif (PAIRING_MODE == LEGACY_PAIRING)
  if (num == 16)
  {
    app_log("16-byte OOB data: ");
    for (uint8_t x = 0; x < 16; x++)
    {
      app_log("0x%02X ", rx_oob_buf[x]);
    }
    app_log("\r\n");

    sc = sl_bt_sm_set_oob_data(16, rx_oob_buf);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set OOB data\n",
                  (int)sc);
    // Set advertising interval to 100ms.
    sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set advertising timing\n",
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
    num = 0;
    _oob_state = STATE_IDLE_MODE;
  }
#endif
}
