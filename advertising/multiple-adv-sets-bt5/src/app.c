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
#include "app_log.h"
#include "sl_simple_led_instances.h"

#define UINT16_TO_BYTES(n) ((uint8_t)(n)), ((uint8_t)((n) >> 8))
#define UINT16_TO_BYTE0(n) ((uint8_t)(n))
#define UINT16_TO_BYTE1(n) ((uint8_t)((n) >> 8))
// The advertising set handle allocated from Bluetooth stack.
static uint8_t handle_ibeacon;
static uint8_t handle_demo;
void bcnSetupAdvBeaconing(void);
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
    sc = sl_bt_advertiser_create_set(&handle_demo);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to create advertising set\n",
                  (int)sc);

    // Set advertising interval to 100ms.
    sc = sl_bt_advertiser_set_timing(
        handle_demo,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set advertising timing\n",
                  (int)sc);

    sl_bt_system_set_tx_power(0, 100, NULL, NULL);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the global minimum and maximum radiated TX power levels for Bluetooth\n",
                  (int)sc);

    /* Set 1 dBm Transmit Power */
    sl_bt_advertiser_set_tx_power(handle_demo, 10, NULL);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to limit the maximum advertising TX power\n",
                  (int)sc);

    // Start general advertising and enable connections.
    sc = sl_bt_extended_advertiser_generate_data(handle_demo,
                                                 sl_bt_advertiser_general_discoverable);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to generate data\n",
                  (int)sc);
    sc = sl_bt_extended_advertiser_start(
        handle_demo,
        sl_bt_extended_advertiser_connectable,
        0);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start advertising\n",
                  (int)sc);

    app_log("\r\nFirst connectable advertising set started.\r\n");
    app_log("  Conected-> LED0 ON else LED0->OFF\r\n");
    bcnSetupAdvBeaconing();
    app_log("\r\nSecond non-connectable advertising set started.\r\n");
    break;

  // -------------------------------
  // This event indicates that a new connection was opened.
  case sl_bt_evt_connection_opened_id:
    sl_led_led0.turn_on(sl_led_led0.context);
    app_log("\r\n connection opened. LED0 status: %s \r\n", sl_led_led0.get_state(sl_led_led0.context) == 1 ? "ON" : "OFF");
    break;

  // -------------------------------
  // This event indicates that a connection was closed.
  case sl_bt_evt_connection_closed_id:
    sl_led_led0.turn_off(sl_led_led0.context);
    app_log("\r\n connection closed. LED0 status: %s \r\n", sl_led_led0.get_state(sl_led_led0.context) == 1 ? "ON" : "OFF");
    // Restart advertising after client has disconnected.
    sc = sl_bt_extended_advertiser_generate_data(handle_demo,
                                                 sl_bt_advertiser_general_discoverable);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to generate data\n",
                  (int)sc);
    sc = sl_bt_extended_advertiser_start(
        handle_demo,
        sl_bt_extended_advertiser_connectable,
        0);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start advertising\n",
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
void bcnSetupAdvBeaconing(void)
{
  sl_status_t sc;

  /* This function sets up a custom advertisement package according to iBeacon specifications.
   * The advertisement package is 30 bytes long. See the iBeacon specification for further details.
   */

  static struct
  {
    uint8_t flagsLen;    /* Length of the Flags field. */
    uint8_t flagsType;   /* Type of the Flags field. */
    uint8_t flags;       /* Flags field. */
    uint8_t mandataLen;  /* Length of the Manufacturer Data field. */
    uint8_t mandataType; /* Type of the Manufacturer Data field. */
    uint8_t compId[2];   /* Company ID field. */
    uint8_t beacType[2]; /* Beacon Type field. */
    uint8_t uuid[16];    /* 128-bit Universally Unique Identifier (UUID). The UUID is an identifier for the company using the beacon*/
    uint8_t majNum[2];   /* Beacon major number. Used to group related beacons. */
    uint8_t minNum[2];   /* Beacon minor number. Used to specify individual beacons within a group.*/
    uint8_t txPower;     /* The Beacon's measured RSSI at 1 meter distance in dBm. See the iBeacon specification for measurement guidelines. */
  } bcnBeaconAdvData = {
      /* Flag bits - See Bluetooth 4.0 Core Specification , Volume 3, Appendix C, 18.1 for more details on flags. */
      2,           /* length  */
      0x01,        /* type */
      0x04 | 0x02, /* Flags: LE General Discoverable Mode, BR/EDR is disabled. */

      /* Manufacturer specific data */
      26,   /* length of field*/
      0xFF, /* type of field */

      /* The first two data octets shall contain a company identifier code from
   * the Assigned Numbers - Company Identifiers document */
      /* 0x004C = Apple */
      {UINT16_TO_BYTES(0x004C)},

      /* Beacon type */
      /* 0x0215 is iBeacon */
      {UINT16_TO_BYTE1(0x0215), UINT16_TO_BYTE0(0x0215)},

      /* 128 bit / 16 byte UUID */
      {0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2, 0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0},

      /* Beacon major number */
      /* Set to 34987 and converted to correct format */
      {UINT16_TO_BYTE1(34987), UINT16_TO_BYTE0(34987)},

      /* Beacon minor number */
      /* Set as 1025 and converted to correct format */
      {UINT16_TO_BYTE1(1025), UINT16_TO_BYTE0(1025)},

      /* The Beacon's measured RSSI at 1 meter distance in dBm */
      /* 0xC3 is -61dBm */
      0xC3};

  //
  uint8_t len = sizeof(bcnBeaconAdvData);
  uint8_t *pData = (uint8_t *)(&bcnBeaconAdvData);

  /* Set custom advertising data */
  sc = sl_bt_advertiser_create_set(&handle_ibeacon);
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to create advertising set\n",
                (int)sc);

  sl_bt_extended_advertiser_set_data(handle_ibeacon, len, pData);
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to set user-defined data\n",
                (int)sc);

  /* Set 8 dBm Transmit Power */
  sl_bt_advertiser_set_tx_power(handle_ibeacon, 80, NULL);
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to limit the maximum advertising TX power\n",
                (int)sc);

  /* Set advertising parameters. 200ms (320/1.6) advertisement interval. All channels used.
   * The first two parameters are minimum and maximum advertising interval, both in
   * units of (milliseconds * 1.6).  */
  sc = sl_bt_advertiser_set_timing(handle_ibeacon, 320, 320, 0, 0);
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to set advertising timing\n",
                (int)sc);

  /* Start advertising in user mode */
  sc = sl_bt_extended_advertiser_start(handle_ibeacon, sl_bt_advertiser_non_connectable, 0);
  app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to start advertising\n",
                (int)sc);
}
