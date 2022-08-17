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
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"

#include "sl_bt_api.h"
#include "app_log.h"

#define SIGNAL_REFRESH_DATA   1
#define TICKS_PER_SECOND      32768

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

sl_sleeptimer_timer_handle_t sleep_timer_handle;
void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
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

  static uint8_t periodic_adv_data[191];
  int16_t result;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      app_log_info("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
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

      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Set TX power
      sc = sl_bt_advertiser_set_tx_power(advertising_set_handle, 30, &result);
      app_assert_status(sc);

      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(advertising_set_handle,
                                       160, // min. adv. interval. Value in units of 0.625 ms
                                       160, // max. adv. interval. Value in units of 0.625 ms
                                       0,   // adv. duration
                                       0);  // max. num. adv. events
      app_assert_status(sc);

      // Start general advertising
      sc = sl_bt_extended_advertiser_generate_data(advertising_set_handle,
                                                 advertiser_general_discoverable);

      app_assert_status(sc);

      sc = sl_bt_extended_advertiser_start(advertising_set_handle,
                                           advertiser_non_connectable,
                                           SL_BT_EXTENDED_ADVERTISER_INCLUDE_TX_POWER );

      app_assert_status(sc);

      // Start periodic advertising with periodic interval 200ms
      sc = sl_bt_periodic_advertiser_start(advertising_set_handle, 160, 160,
                                           SL_BT_PERIODIC_ADVERTISER_AUTO_START_EXTENDED_ADVERTISING);
      app_assert_status(sc);

      // Set the advertising data
      sc = sl_bt_periodic_advertiser_set_data(advertising_set_handle, sizeof(periodic_adv_data), periodic_adv_data);

      app_assert_status(sc);

      // Start a timer to change advertising data every 1s
      sc = sl_sleeptimer_start_periodic_timer(&sleep_timer_handle, TICKS_PER_SECOND, sleeptimer_callback, (void*)NULL, 0, 0);

      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    case sl_bt_evt_system_external_signal_id:
      if(evt->data.evt_system_external_signal.extsignals == SIGNAL_REFRESH_DATA)
      {
        app_log_info("\r\n");
        for (int i = 0; i < 190; i++) {
            periodic_adv_data[i] = rand()%9;
            app_log_info(" %X", periodic_adv_data[i]);
        }
        app_log_info("\r\n");

        sc = sl_bt_periodic_advertiser_set_data(advertising_set_handle, sizeof(periodic_adv_data), periodic_adv_data);
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

/***************************************************************************//**
 * Sleeptimer callback
 *
 * Note: This function is called from interrupt context
 *
 * @param[in] handle Handle of the sleeptimer instance
 * @param[in] data  Callback data
 ******************************************************************************/
void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data){
  (void)handle;
  (void)data;

  sl_bt_external_signal(SIGNAL_REFRESH_DATA);

}
