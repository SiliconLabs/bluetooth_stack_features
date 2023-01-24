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
#include "nvm3.h"

/* First NVM key used for storing database caches */
#define NVM_KEY_DATABASE_CACHE_BASE    0x00001

/* Maximum number of stored database caches */
#define MAX_NUM_OF_CACHES           5

/* UUID of the Generic Attribute Service */
static uint8_t generic_access_uuid[2] = {0x01,0x18};

/* UUID of the Service Changed characteristic */
static uint8_t service_changed_uuid[2] = {0x05,0x2a};

/* connection handle */
static uint8_t conn_handle = 0xFF;
static uint8_t bonding_handle = 0xFF;
static bd_addr server_address;

typedef struct {
  bd_addr  address;                   /* address of the GATT server for which this database is stored */
  uint32_t generic_attribute_handle;  /* Generic Attribute service handle */
  uint16_t service_changed_handle;    /* Service Changed characteristic handle */
  /* add here additional handles used by application */
} db_cache_t;

/* database cache in which handles are stored */
static db_cache_t db_cache = {{{0,0,0,0,0,0}}, 0xFFFFFFFF, 0xFFFF};

/* state variables */
static uint8_t waiting_for_service_discovery_to_finish = 0;
static uint8_t waiting_for_characteristic_discovery_to_finish = 0;

/**************************************************************************//**
 * decoding advertising packets is done here. The list of AD types can be found
 * at: https://www.bluetooth.com/specifications/assigned-numbers/Generic-Access-Profile
 *
 * @param[in] pReso  Pointer to a scan report event
 * @param[in] name   Pointer to the name which is looked for
 *****************************************************************************/
static uint8_t findDeviceByName(sl_bt_evt_scanner_scan_report_t *pResp, char* name)
{
  uint8_t i = 0;
  uint8_t ad_len,ad_type;

  while (i < (pResp->data.len - 1))
  {
    ad_len  = pResp->data.data[i];
    ad_type = pResp->data.data[i+1];

    if (ad_type == 0x08 || ad_type == 0x09 )
    {
      // type 0x08 = Shortened Local Name
      // type 0x09 = Complete Local Name
      if (memcmp(name, &(pResp->data.data[i+2]), ad_len-1) == 0) {
        return 1;
      }
    }
    //jump to next AD record
    i = i + ad_len + 1;
  }
  return 0;
}

/**************************************************************************//**
 * Loads cached service and characteristic handles
 * The function will loop through the reserved NVM keys, and will copy
 * the database cache to the input pointer if found, otherwise returns 0
 *
 * @param[in] address     BLE address of the remote device
 * @param[in] p_db_cache  Pointer to the database cache
 *****************************************************************************/
static uint8_t loadDatabaseCache(bd_addr address, db_cache_t* p_db_cache)
{
  uint8_t result = 0;
  uint16_t key;
  Ecode_t ret_code;
  db_cache_t temp;

  /* iterate through user keys in the NVM */
  for (key = NVM_KEY_DATABASE_CACHE_BASE; key < (NVM_KEY_DATABASE_CACHE_BASE + MAX_NUM_OF_CACHES); key++) {
    ret_code = nvm3_readData(nvm3_defaultHandle, key, &temp, sizeof(db_cache_t));
    /* if loading error, return false */
    if (ret_code != ECODE_NVM3_OK){
        result = 0;
        break;
    } else{
      /* check device address, if it matches the required address, load the DB cache */
      if (memcmp(&temp.address.addr[0] , &address.addr[0], 6) == 0) {
          memcpy(p_db_cache, &temp, sizeof(db_cache_t));
          app_log("database cache loaded from key 0x%04X\r\n", key);
          result = 1;
      }
    }
  }
  return result;
}

/**************************************************************************//**
 * Saves cached service and characteristic handles
 * The function will loop through the reserved NVM keys, and will either
 *  - store the data to the first empty key
 *  - or will overwrite the already saved one if an entry with the same address is found
 *
 * @param[in] address     BLE address of the remote device
 * @param[in] p_db_cache  Pointer to the database cache
 *****************************************************************************/
static uint8_t storeDatabaseCache(bd_addr address, db_cache_t* p_db_cache)
{
  uint8_t result;
  Ecode_t ret_code;
  uint16_t key;
  db_cache_t temp;

  /* iterate through user keys in the NVM */
  for (key = NVM_KEY_DATABASE_CACHE_BASE; key < (NVM_KEY_DATABASE_CACHE_BASE + MAX_NUM_OF_CACHES); key++) {
     ret_code = nvm3_readData(nvm3_defaultHandle, key, &temp, sizeof(db_cache_t));
    if(ret_code == ECODE_NVM3_ERR_KEY_NOT_FOUND){
      /* if key not found then it's the end of existing keys, store new data here */
      break;
    }
      /* check device address, if it matches the required address, store new data here */
    if (memcmp(&temp.address , &address.addr[0], 6) == 0) {
      break;
     }
   }
   ret_code = nvm3_writeData(nvm3_defaultHandle, key, p_db_cache, sizeof(db_cache_t));

  if (ret_code != ECODE_NVM3_OK){
    result = 0;
  }else{
    result = 1;
    app_log("database cache stored to key 0x%04X\r\n", key);
  }
  return result;
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      /* Enable bondings in security manager (this is needed for Service Change Indications) */
      sl_bt_sm_configure( 2, sl_bt_sm_io_capability_noinputnooutput);

      /* 10ms scan interval, 100% duty cycle, 1M PHY*/
      sl_bt_scanner_set_timing(gap_1m_phy, 16, 16);

      sl_bt_scanner_start(gap_1m_phy, sl_bt_scanner_discover_observation);
      break;

    case sl_bt_evt_scanner_scan_report_id:
      /* Find server by name */
      if (findDeviceByName(&(evt->data.evt_scanner_scan_report),"GATT server")) {
        /* Connect to server */
        sl_bt_connection_open(evt->data.evt_scanner_scan_report.address, evt->data.evt_scanner_scan_report.address_type, gap_1m_phy, &conn_handle);
      }
      break;
    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("connection opened\r\n");
      bonding_handle = evt->data.evt_connection_opened.bonding;
      server_address = evt->data.evt_connection_opened.address;

      /* if we are not bonded with the server yet, then we should create bonding. */
      if(bonding_handle == 0xFF){
        sc = sl_bt_sm_increase_security(evt->data.evt_connection_opened.connection);
        app_log("initialize bonding, %x\r\n", sc);
      }/* otherwise, if bonding already exists, we can load the stored database cache */
      else {
        app_log("device is already bonded\r\n");
        if (!loadDatabaseCache(server_address, &db_cache)){
          /* if loading failed, discover the database again. */
          sl_bt_gatt_discover_primary_services(conn_handle);
        }
      }
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log("bonding created\r\n");
      /* if bonding is successful, start discovering services */
      sl_bt_gatt_discover_primary_services(conn_handle);
    break;

    case sl_bt_evt_gatt_service_id:
      /* if we found the Generic Attribute Service */
      if (memcmp(evt->data.evt_gatt_service.uuid.data , generic_access_uuid, sizeof(generic_access_uuid)) == 0) {
        /* save handle and wait for process to be finished */
        db_cache.generic_attribute_handle = evt->data.evt_gatt_service.service;
        waiting_for_service_discovery_to_finish = 1;
      }
      break;

    case sl_bt_evt_gatt_characteristic_id:
      /* if we found the Service Changed characteristic */
      if (memcmp(evt->data.evt_gatt_characteristic.uuid.data , service_changed_uuid, sizeof(service_changed_uuid)) == 0) {
        /* save handle and wait for process to be finished */
        db_cache.service_changed_handle = evt->data.evt_gatt_characteristic.characteristic;
        waiting_for_characteristic_discovery_to_finish = 1;
      }
      break;

    case sl_bt_evt_gatt_procedure_completed_id:
      /* if service discovery finished */
      if (waiting_for_service_discovery_to_finish) {
        app_log("service discovery finished\r\n");
        waiting_for_service_discovery_to_finish = 0;
        /* discover service changed characteristic */
        sl_bt_gatt_discover_characteristics_by_uuid(evt->data.evt_gatt_procedure_completed.connection,
                                                    db_cache.generic_attribute_handle,
                                                    sizeof(service_changed_uuid),
                                                    service_changed_uuid);
      }

      /* if characteristic discovery finished */
      if (waiting_for_characteristic_discovery_to_finish) {
        app_log("characteristic discovery finished\r\n");
        waiting_for_characteristic_discovery_to_finish = 0;
        /* store database cache */
        db_cache.address = server_address;
        storeDatabaseCache(server_address, &db_cache);
          /* subscribe for indications */
          sl_bt_gatt_set_characteristic_notification(evt->data.evt_gatt_procedure_completed.connection,
                                                     db_cache.service_changed_handle,
                                                     gatt_indication);
      }

      break;

    case sl_bt_evt_gatt_characteristic_value_id:
      /* if we got a service changed indication */
      if (evt->data.evt_gatt_characteristic_value.characteristic == db_cache.service_changed_handle) {
            app_log("Service Change Indication received. Changes are in the range: 0x%04X - 0x%04X\r\n",
                              *((uint16_t*)&evt->data.evt_gatt_characteristic_value.value.data[0]),
                              *((uint16_t*)&evt->data.evt_gatt_characteristic_value.value.data[2]));
        /* send back confirmation */
        sl_bt_gatt_send_characteristic_confirmation(evt->data.evt_gatt_characteristic_value.connection);
        /* and initiate new database discovery */
        sl_bt_gatt_discover_primary_services(conn_handle);
      }
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
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
