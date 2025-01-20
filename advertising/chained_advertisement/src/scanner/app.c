/***************************************************************************//**
 * @file scanner
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

#include "sl_bt_api.h"
#include "app_log.h"

// This constant is UUID of periodic synchronous service
const uint8_t periodicSyncService[16] = {0x81,0xc2,0x00,0x2d,0x31,0xf4,0xb0,0xbf,0x2b,0x42,0x49,0x68,0xc7,0x25,0x71,0x41};

// Parse advertisements looking for advertised periodicSync Service.
static uint8_t find_service_in_advertisement(uint8_t *data, uint8_t len)
{
  uint8_t adFieldLength;
  uint8_t adFieldType;
  uint8_t i = 0;
  app_log_info("packet length %d\r\n", len);
  // Parse advertisement packet
  while (i < len) {
    adFieldLength = data[i];
    adFieldType = data[i + 1];
    // Partial ($02) or complete ($03) list of 128-bit UUIDs
  app_log_info("adField type %d \r\n", adFieldType);
    if (adFieldType == 0x06 || adFieldType == 0x07) {
      // compare UUID to service UUID
      if (memcmp(&data[i + 2], periodicSyncService, 16) == 0) {
        return 1;
      }
    }
    // advance to the next AD struct
    i = i + adFieldLength + 1;
  }
  return 0;
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

  static uint16_t sync;

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

      app_log("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
            address_type ? "static random" : "public device",
            address.addr[5],
            address.addr[4],
            address.addr[3],
            address.addr[2],
            address.addr[1],
            address.addr[0]);

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

      app_log_info("Scanner boot event\n");

      // periodic scanner setting
      sl_bt_scanner_set_parameters(sl_bt_scanner_scan_mode_passive, 200, 200);
      sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m ,
                          sl_bt_scanner_discover_observation );

      break;

    // scan response
    case sl_bt_evt_scanner_extended_advertisement_report_id:
      app_log_info("got ext adv indication with tx_power = %d\r\n",
      evt->data.evt_scanner_extended_advertisement_report.tx_power );
      if (find_service_in_advertisement(&(evt->data.evt_scanner_extended_advertisement_report.data.data[0]),
                                       evt->data.evt_scanner_extended_advertisement_report.data.len) != 0) {

       app_log_info("found periodic sync service, attempting to open sync\r\n");

       sc = sl_bt_sync_scanner_open (evt->data.evt_scanner_extended_advertisement_report.address,
                            evt->data.evt_scanner_extended_advertisement_report.address_type,
                            evt->data.evt_scanner_extended_advertisement_report.adv_sid,
                            &sync);
       app_log_info("cmd_sync_open() sync = 0x%4lX\r\n", sc);
      }
      break;
 
    case sl_bt_evt_periodic_sync_opened_id:
      /* now that sync is open, we can stop scanning*/
      app_log_info("evt_sync_opened\r\n");
      sl_bt_scanner_stop();
      break;

    case sl_bt_evt_sync_closed_id:
       app_log_info("periodic sync closed. reason 0x%2X, sync handle %d",
                  evt->data.evt_sync_closed.reason,
                  evt->data.evt_sync_closed.sync);
       /* restart discovery */
       sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m,
                           sl_bt_scanner_discover_observation);
       break;

     case sl_bt_evt_periodic_sync_report_id:
       {
         static uint16_t sync_data_index = 0;
         static uint8_t sync_rx_buffer[1650];
         uint16_t len = evt->data.evt_periodic_sync_report.data.len;

         switch(evt->data.evt_periodic_sync_report.data_status)
           {
           case 0:
             app_log_info("complete sync %d bytes received.\r\n",
                        evt->data.evt_periodic_sync_report.data.len);
             memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_periodic_sync_report.data.data, len);
             sync_data_index = 0;
             break;
           case 1:
             /* */
             app_log_info("sync data received, %d bytes, more to come\r\n",
                        evt->data.evt_periodic_sync_report.data.len);
             memcpy(&sync_rx_buffer[sync_data_index], evt->data.evt_periodic_sync_report.data.data, len);
             sync_data_index += len;
             break;
           case 2:
             app_log_info("data corrupted, discard entire sync. sync_data_index was %d and %d bytes received in this event\r\n",
                        sync_data_index,
                        len);
             memset(sync_rx_buffer,0,sizeof(sync_rx_buffer));
             sync_data_index = 0;
             break;
           }
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
