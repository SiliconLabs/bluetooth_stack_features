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
#include "sl_sleeptimer.h"
/**************************************************************************//**
 * RfSense specific includes
 *****************************************************************************/
#include "rail.h"
#include "em_emu.h"

/**************************************************************************//**
 * RfSense specific definitions
 *****************************************************************************/
//Callback function forward declaration
void rfsense_callback();

// RFSENSE Selective Configuration
RAIL_RfSenseSelectiveOokConfig_t rfsense_config = {
  .band = RAIL_RFSENSE_2_4GHZ, //RAIL_RFSENSE_ANY, //RAIL_RFSENSE_OFF RAIL_RFSENSE_2_4GHZ RAIL_RFSENSE_2_4GHZ_LOW_SENSITIVITY RAIL_RFENSE_ANY_LOW_SENSITIVITY
  .syncWordNumBytes = 2, //NUMSYNCWORDBYTES,
  .syncWord = 0xB16F, //SYNCWORD,
  .cb = &rfsense_callback //NULL //&RAILCb_RfSense
};

#define RFSENSE_MODE_LEGACY 0
#define RFSENSE_MODE_SELECTIVE 1

#define RFSENSE_SELECTED_MODE RFSENSE_MODE_LEGACY


#define RFSENSE_WAIT (5*32768) //5 sec
#define RFSENSE_TIME 1000 //1000 msec is the minimum for BG22
#define RFSENSE_EXT_SIGNAL 1

sl_sleeptimer_timer_handle_t rfsense_timer;
static bool goToDeepSleep = true;
static RAIL_Handle_t railHandleRfsense = RAIL_EFR32_HANDLE;

//RfSense callback
void rfsense_callback()
{
  app_log("RfSense callback!\r\n");
  return;
 }


// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

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
 Callback for the sleeptimer. Since this function is called from interrupt context,
 only an external signal is set, as no other BGAPI calls are allowed in this case.
 *****************************************************************************/
void sleep_timer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)data;
  (void)handle;
  sl_bt_external_signal(RFSENSE_EXT_SIGNAL);
}

/**************************************************************************//**
Function for starting RfSense
 *****************************************************************************/
void app_rfsense_handler(void)
{
  RAIL_Status_t rail_status;
  RAIL_Time_t rfsense_time;

   if (RFSENSE_MODE_SELECTIVE == RFSENSE_SELECTED_MODE){
       rail_status =  RAIL_StartSelectiveOokRfSense(railHandleRfsense, &rfsense_config);
       app_log("\r\nSelective RfSense start status: %x \r\n", rail_status);
   }
   else{
       rfsense_time = RAIL_StartRfSense(RAIL_EFR32_HANDLE, RAIL_RFSENSE_2_4GHZ, RFSENSE_TIME, rfsense_callback);
       app_log("\r\nLegacy RfSense started with RfSense time: %d\r\n", rfsense_time);
   }

 if(goToDeepSleep)
  {
     EMU_EnterEM4(); //if device goes to EM4, the callback function won't be called, a reset event will happen instead
  }
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

      //Start a timer, the RfSense will be activated when it expires
      sl_sleeptimer_start_timer(&rfsense_timer, RFSENSE_WAIT, sleep_timer_callback, NULL, 0, 0);
      break;

    case sl_bt_evt_system_external_signal_id:
      if(evt->data.evt_system_external_signal.extsignals & RFSENSE_EXT_SIGNAL) {
          app_rfsense_handler();
      }
      break;
    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      // stop the timer if a connection is opened to prevent going to EM4
      sl_sleeptimer_stop_timer(&rfsense_timer);
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

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
