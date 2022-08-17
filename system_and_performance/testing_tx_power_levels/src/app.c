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
#include "app_log.h"

#define MIN_TEST_NUMBER         (-300) // This is the minimum tx_power to start the sweep.
#define MAX_TEST_NUMBER         (80)   // This is the maximum tx_power to end the sweep.

#define PRINT_INTERVAL          328 //10 msec
#define SIGNAL_PRINT             1

static int16_t tx_set;
static int16_t tx_to_set = MIN_TEST_NUMBER;

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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      /* Print stack version. */
      app_log_info("Bluetooth stack booted: v%d.%d.%d-b%d\n",
                 evt->data.evt_system_boot.major,
                 evt->data.evt_system_boot.minor,
                 evt->data.evt_system_boot.patch,
                 evt->data.evt_system_boot.build);

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      /* Print local Bluetooth address. */
      app_log_info("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 address_type ? "static random" : "public device",
                 address.addr[5],
                 address.addr[4],
                 address.addr[3],
                 address.addr[2],
                 address.addr[1],
                 address.addr[0]);

      app_log_info("------------------------Start-------------------------\r\n");

      /* Set a timer to call sl_bt_system_set_tx_power every 10 ms
       * with incremental input and print out result. */
      sc = sl_sleeptimer_start_periodic_timer(&sleep_timer_handle, PRINT_INTERVAL, sleeptimer_callback, (void*)NULL, 0, 0);
      app_assert_status(sc);
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals == SIGNAL_PRINT) {

        /* Set maximum tx power and read what was actually set */
        sc = sl_bt_system_set_tx_power(MIN_TEST_NUMBER, tx_to_set, NULL, &tx_set);
        app_assert_status(sc);
        /* Print out input value and response from the command.
         * The print formatting can be changed as desired
         * (e.g. so that a log can be imported as csv into excel).
         */
        app_log_info("set_tx_power(%03d) returns %03d\r\n", tx_to_set, tx_set);

        if (tx_to_set++ == MAX_TEST_NUMBER) {
          /* If max value has been reached print "End" and stop timer */
          app_log_info("-----------------------End-------------------------\r\n");
          sc = sl_sleeptimer_stop_timer(&sleep_timer_handle);
          app_assert_status(sc);
        }
      }
      break;

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

  sl_bt_external_signal(SIGNAL_PRINT);

}
