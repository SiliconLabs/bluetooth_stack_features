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
#include "sl_app_log.h"
#include "mbedtls/md.h"

//#define USE_RANDOM_PUBLIC_ADDRESS

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

#ifdef USE_RANDOM_PUBLIC_ADDRESS
static void set_random_public_address(void);
#endif
static uint32_t make_passkey_from_address(bd_addr address);

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
  uint32_t passkey = 0;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      /* Print stack version */
      sl_app_log("Bluetooth stack booted: v%d.%d.%d-b%d\n",
                 evt->data.evt_system_boot.major,
                 evt->data.evt_system_boot.minor,
                 evt->data.evt_system_boot.patch,
                 evt->data.evt_system_boot.build);

#ifdef USE_RANDOM_PUBLIC_ADDRESS
      set_random_public_address();
#endif

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to get Bluetooth address\n",
                    (int)sc);

      /* Print Bluetooth address */
      sl_app_log("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       address_type ? "static random" : "public device",
                       address.addr[5],
                       address.addr[4],
                       address.addr[3],
                       address.addr[2],
                       address.addr[1],
                       address.addr[0]);

      passkey = make_passkey_from_address(address);
      sc = sl_bt_sm_configure(0x07, sl_bt_sm_io_capability_displayonly);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to configure security\n",
                    (int)sc);

      sc = sl_bt_sm_set_passkey(passkey);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set passkey\n",
                    (int)sc);

      sc = sl_bt_sm_set_bondable_mode(1);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set bondalbe mode \n",
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

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to create advertising set\n",
                    (int)sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising timing\n",
                    (int)sc);
      // Start general advertising and enable connections.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      sl_app_log("Connection opened\r\n");
      sc = sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to increasing security\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Restart advertising after client has disconnected.
      sc = sl_bt_advertiser_start(
        advertising_set_handle,
        advertiser_general_discoverable,
        advertiser_connectable_scannable);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to start advertising\n",
                    (int)sc);
      break;

    // -------------------------------
    // This event indicates a request to display the passkey to the user.
    case sl_bt_evt_sm_passkey_display_id:
      sl_app_log("passkey: %4d\r\n", evt->data.evt_sm_passkey_display.passkey);
      break;

    // -------------------------------
    // This event triggered after the pairing or bonding procedure is
    // successfully completed.
    case sl_bt_evt_sm_bonded_id:
      sl_app_log("bond success\r\n");
      break;

    // -------------------------------
    // This event is triggered if the pairing or bonding procedure fails.
    case sl_bt_evt_sm_bonding_failed_id:
      sl_app_log("bonding failed, reason 0x%2X\r\n",
                 evt->data.evt_sm_bonding_failed.reason);
      sc = sl_bt_connection_close(evt->data.evt_sm_bonding_failed.connection);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to close a Bluetooth connection\n",
                    (int)sc);
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

static uint32_t make_passkey_from_address(bd_addr address)
{
  mbedtls_md_context_t md;
  const mbedtls_md_info_t *md_info;
  uint8_t hash_buffer[32];
  int error;
  uint32_t passkey = 0;

  /* hash the result*/
  mbedtls_md_init(&md);
  md_info = mbedtls_md_info_from_string("SHA256");
  if (md_info == NULL) {
    sl_app_log("NULL md info\r\n");
    return 0;
  }
  error = mbedtls_md(md_info, address.addr, sizeof(address), hash_buffer);
  if (error) {
    sl_app_log("hash error %d\r\n", error);
  }
  memcpy(&passkey, hash_buffer, sizeof(passkey));
  /* restrict to 6 digits */
  passkey %= 1000000;
  return passkey;
}

/*
 * This function creates a new random static address and sets it to be
 * the device's identity address
 *
 */
#ifdef USE_RANDOM_PUBLIC_ADDRESS
static void set_random_public_address(void)
{
  sl_status_t sc;
  bd_addr address;
  size_t data_len;
  uint8_t data[16];

  sc = sl_bt_system_get_random_data(6, sizeof(data), &data_len, data);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to get random data\n",
                (int)sc);

  memcpy(address.addr, data, sizeof(bd_addr));
  /* set uppermost 2 bits to make this a random static address */
  address.addr[5] |= 0xC0;

  sc = sl_bt_system_set_identity_address(address, sl_bt_gap_static_address);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to set identity address\n",
                (int)sc);
}
#endif
