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
#include "nvm3.h"

#define NVM_KEY_HASH_1      0x01
#define NVM_KEY_HASH_2      0x02

#define DATABASE_HASH_LENGTH   16

#define WRITE_VALUE_TIMEOUT       1

sl_sleeptimer_timer_handle_t write_value_timeout_timer;

/* UUID of the DB hash characteristic */
static uint8_t db_hash_uuid[2] = { 0x2a, 0x2b };

/* DB hash of the 1st version of the database */
static uint8_t db_hash_version_1[DATABASE_HASH_LENGTH];

/* DB hash of the 2nd version of the database */
static uint8_t db_hash_version_2[DATABASE_HASH_LENGTH];

/* characteristic handle of the Client Supported Features characteristic in version 1 of the GATT database */
static uint16_t client_supported_features_handle_version_1 = 8;

/* characteristic handle of the Client Supported Features characteristic in version 2 of the GATT database */
static uint16_t client_supported_features_handle_version_2 = 8;

/* characteristic handle of the custom characteristic in version 1 of the GATT database */
static uint16_t char_handle_version_1 = 23;

/* characteristic handle of the custom characteristic in version 2 of the GATT database */
static uint16_t char_handle_version_2 = 26;

/* version number of the remote DB. 0xFF means unknown version */
static uint8_t db_version = 0xFF;

/* connection handle */
static uint8_t conn_handle = 0xFF;

/* enable bit to be sent */
static uint8_t enable_bit[1] = { 0x01 };

/* dummy data to be sent */
static uint8_t dummy_data[1] = { 0x01 };

/* remember if we have already enabled robust caching */
static uint8_t robust_caching_enabled = 0;

static uint8_t valid_handles = 0;

/**************************************************************************//**
   Callback for the sleeptimer.
 *****************************************************************************/
void sleep_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)data;
  (void)handle;
  sl_bt_external_signal(WRITE_VALUE_TIMEOUT);
}

/**************************************************************************//**
 * decoding advertising packets is done here. The list of AD types can be found
 * at: https://www.bluetooth.com/specifications/assigned-numbers/Generic-Access-Profile
 *
 * @param[in] pReso  Pointer to a scan report event
 * @param[in] name   Pointer to the name which is looked for
 *****************************************************************************/
static uint8_t findDeviceByName(sl_bt_evt_scanner_legacy_advertisement_report_t *pResp, char* name)
{
  uint8_t i = 0;
  uint8_t ad_len, ad_type;

  while (i < (pResp->data.len - 1)) {
    ad_len  = pResp->data.data[i];
    ad_type = pResp->data.data[i + 1];

    if (ad_type == 0x08 || ad_type == 0x09 ) {
      // type 0x08 = Shortened Local Name
      // type 0x09 = Complete Local Name
      if (memcmp(name, &(pResp->data.data[i + 2]), ad_len - 1) == 0) {
        return 1;
      }
    }
    //jump to next AD record
    i = i + ad_len + 1;
  }
  return 0;
}
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void)
{
  Ecode_t ret_code;

  ret_code = nvm3_readData(nvm3_defaultHandle, NVM_KEY_HASH_1, db_hash_version_1, DATABASE_HASH_LENGTH);
  if (ret_code == ECODE_NVM3_OK) {
    valid_handles++;
    ret_code = nvm3_readData(nvm3_defaultHandle, NVM_KEY_HASH_2, db_hash_version_2, DATABASE_HASH_LENGTH);
    if (ret_code == ECODE_NVM3_OK) {
      valid_handles++;
    }
  }
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
  Ecode_t ret_code;
  nvm3_ObjectKey_t key;
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      app_log("Boot event\r\n");
      /* Enable bondings in security manager (this is needed for Service Change Indications) */
      sc = sl_bt_sm_configure(2, sl_bt_sm_io_capability_noinputnooutput);
      app_assert_status(sc);

      /* 10ms scan interval, 100% duty cycle*/
      sc = sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive, 16, 16);
      app_assert_status(sc);

      sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_observation);
      app_assert_status(sc);
      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      /* Find server by name */
      if (findDeviceByName(&evt->data.evt_scanner_legacy_advertisement_report, "GATT server")) {
        /* Connect to server */
        sc = sl_bt_connection_open(evt->data.evt_scanner_legacy_advertisement_report.address, evt->data.evt_scanner_legacy_advertisement_report.address_type, sl_bt_gap_1m_phy, &conn_handle);
        app_assert_status(sc);
      }
      break;

    case sl_bt_evt_connection_opened_id:
      app_log("connection opened\r\n");
      /* store connection handle for future use */
      conn_handle = evt->data.evt_connection_opened.connection;
      /* read database hash */
      sc = sl_bt_gatt_read_characteristic_value_by_uuid(evt->data.evt_connection_opened.connection, 0x0001FFFF, sizeof(db_hash_uuid), &db_hash_uuid[0]);
      app_assert_status(sc);
      break;

    case sl_bt_evt_gatt_characteristic_value_id:
      /* if a read_by_type response is received, then it will be the database hash,
       * as in this application this is the only value that we read by UUID and not by handle */
      if (evt->data.evt_gatt_characteristic_value.att_opcode == sl_bt_gatt_read_by_type_response) {
        /* print DB hash */
        app_log("database hash: ");
        for (uint8_t i = 0; i < 16; i++) {
          app_log("%02X", evt->data.evt_gatt_characteristic_value.value.data[i]);
        }
        app_log("\r\n");

        /* Let's compare the hash with the two version of the database we know from earlier observations */
        if (memcmp(&evt->data.evt_gatt_characteristic_value.value.data[0], &db_hash_version_1[0], 16) == 0) {
          db_version = 1;
          app_log("version 1 detected\r\n");
        } else
        if (memcmp(&evt->data.evt_gatt_characteristic_value.value.data[0], &db_hash_version_2[0], 16) == 0) {
          db_version = 2;
          app_log("version 2 detected\r\n");
        } else {
          db_version = 0xFF;
          app_log("unknown version detected, storing it to NVM\r\n");
          if (valid_handles == 0) {
            key = NVM_KEY_HASH_1;
            memcpy(db_hash_version_1, evt->data.evt_gatt_characteristic_value.value.data, DATABASE_HASH_LENGTH);
          } else {
            key = NVM_KEY_HASH_2;
            memcpy(db_hash_version_2, evt->data.evt_gatt_characteristic_value.value.data, DATABASE_HASH_LENGTH);
          }
          //store the value to the NVM
          ret_code = nvm3_writeData(nvm3_defaultHandle, key, evt->data.evt_gatt_characteristic_value.value.data, DATABASE_HASH_LENGTH);
          valid_handles++;
          if (ret_code != ECODE_NVM3_OK) {
            app_log("Error while storing data to the NVM: %x\r\n", ret_code);
          }
          db_version = valid_handles;
        }

        /* enable robust caching by writing 0x01 into the Client Supported Features characteristic */
        if (robust_caching_enabled == 0) {
          /* The handle of the Client Supported Features characteristic depends on a version of the remote GATT database.
           * Here we assume, that we already know the handle for each version from earlier observations. */
          if (db_version == 1) {
            app_log("enable robust caching... ");
            sc = sl_bt_gatt_write_characteristic_value(conn_handle, client_supported_features_handle_version_1, sizeof(enable_bit), &enable_bit[0]);
            app_assert_status(sc);
          } else
          if (db_version == 2) {
            app_log("enable robust caching... ");
            sc = sl_bt_gatt_write_characteristic_value(conn_handle, client_supported_features_handle_version_2, sizeof(enable_bit), &enable_bit[0]);
            app_assert_status(sc);
          }
          robust_caching_enabled = 1;
        }

        /* After detecting the database let's write a dummy value to the custom characteristic every second.
         * Start a sleeptimer here that will trigger the write procedure */
        sc = sl_sleeptimer_start_periodic_timer_ms(&write_value_timeout_timer, 1000, sleep_timer_callback, NULL, 0, 0);
        app_assert_status(sc);
      }
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & WRITE_VALUE_TIMEOUT) {
        /* Write a dummy value to the custom characteristic. The handle of the custom characteristic depends
         * on a version of the remote GATT database. Here we assume, that we already know the handle for each
         * version from earlier observations. */
        if (db_version == 1) {
          app_log("write to handle %d... ", char_handle_version_1);
          sc = sl_bt_gatt_write_characteristic_value(conn_handle, char_handle_version_1, sizeof(dummy_data), &dummy_data[0]);
          app_assert_status(sc);
        } else
        if (db_version == 2) {
          app_log("write to handle %d... ", char_handle_version_2);
          sc = sl_bt_gatt_write_characteristic_value(conn_handle, char_handle_version_2, sizeof(dummy_data), &dummy_data[0]);
          app_assert_status(sc);
        } else {
          /* we do not know the handle for the characteristic, so we should discover it! This is outside of the scope
           * of this example. Find an example for GATT discovery. */
        }
      }
      break;

    case sl_bt_evt_gatt_procedure_completed_id:
      /* check the result of the read/write procedure */
      if (evt->data.evt_gatt_procedure_completed.result == SL_STATUS_OK) {
        app_log("OK\r\n");
      } else
      /* check if out-of-sync error was received */
      if (evt->data.evt_gatt_procedure_completed.result == SL_STATUS_BT_ATT_OUT_OF_SYNC) {
        app_log("database has changed!\r\n");

        // Stop sleeptimer
        sc = sl_sleeptimer_stop_timer(&write_value_timeout_timer);
        app_assert_status(sc);

        /* read database hash */
        sc = sl_bt_gatt_read_characteristic_value_by_uuid(conn_handle, 0x0001FFFF, sizeof(db_hash_uuid), &db_hash_uuid[0]);
        app_assert_status(sc);
      } else {
        app_log("ERROR: 0x%04x\r\n", evt->data.evt_gatt_procedure_completed.result);
      }
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      robust_caching_enabled = 0;
      sc = sl_sleeptimer_stop_timer(&write_value_timeout_timer);
      if (sc != SL_STATUS_INVALID_STATE) {
        //SL_INVALID_STATE indicates that the timer was already stopped
        app_assert_status(sc);
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
