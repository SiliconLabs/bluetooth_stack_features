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

#include "sl_bt_api.h"
#include "sl_app_log.h"


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

  uint8_t app_connection;
  bd_addr *remote_address;
  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      sl_app_assert(sc == SL_STATUS_OK,
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
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to write attribute\n",
                    (int)sc);


      sl_app_log("System Boot\r\n");

      // set scan mode
      sc = sl_bt_scanner_set_mode(gap_coded_phy,
                             0); // 0 mean passive scanning mode
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set scanner mode \n",
                    (int)sc);

      // set scan timming
      sc = sl_bt_scanner_set_timing(gap_coded_phy,
                                    200,
                                    200);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set scanner timming \n",
                    (int)sc);

      // start scanning
      sl_bt_scanner_start(gap_coded_phy,
                          scanner_discover_observation);

      // start the soft timer 1 to notice if cannot find any advertiser
      sl_bt_system_set_soft_timer(327680,
                                  1,
                                  1);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      sl_bt_scanner_stop();
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      sl_bt_scanner_start(gap_coded_phy,
                          scanner_discover_observation);
      break;

    // soft timer event to stop the scanner
    case sl_bt_evt_system_soft_timer_id:
      if(1==evt->data.evt_system_soft_timer.handle){
          sl_bt_scanner_stop();
          sl_app_log("no connectable devices in range\r\n");
      }
      break;

    // scan response
    case sl_bt_evt_scanner_scan_report_id:

      // there is no filter here because the LE Coded PHY device is rare
      // add the filter if it is needed
      remote_address = &(evt->data.evt_scanner_scan_report.address);

      sl_app_log("advertisement/scan response from %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X\r\n",
             remote_address->addr[5],
             remote_address->addr[4],
             remote_address->addr[3],
             remote_address->addr[2],
             remote_address->addr[1],
             remote_address->addr[0]);

      sl_app_log("RSSI %d\r\n", evt->data.evt_scanner_scan_report.rssi);


      sl_app_log("data len: %d\r\n", evt->data.evt_scanner_scan_report.data.len);

      sl_app_log("\r\n data raw: \r\n");
      for(int i = 0; i< evt->data.evt_scanner_scan_report.data.len; i++){
          sl_app_log("%c ", evt->data.evt_scanner_scan_report.data.data[i]);
      }

      sl_app_log("\r\n data hex: \r\n");
      for(int i = 0; i< evt->data.evt_scanner_scan_report.data.len; i++){
          sl_app_log("%x ", evt->data.evt_scanner_scan_report.data.data[i]);
      }

      //stop the tracking timer
      sl_bt_system_set_soft_timer(0,
                                  1,
                                  0);
      sl_bt_scanner_stop();
      sc = sl_bt_connection_open(*remote_address,
                            sl_bt_gap_public_address,
                            gap_coded_phy,
                            &app_connection);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to open connection\n",
                    (int)sc);
      break;

    case sl_bt_evt_connection_phy_status_id:
      sl_app_log("using PHY %d\r\n", evt->data.evt_connection_phy_status.phy );
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
