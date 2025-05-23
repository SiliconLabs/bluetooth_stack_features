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
#include "em_gpio.h"
#include "em_prs.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

#define RX_OBS_PRS_CHANNEL 0
#define TX_OBS_PRS_CHANNEL 1

void enableDebugGpios(GPIO_Port_TypeDef rx_obs_port, uint8_t rx_obs_pin, GPIO_Port_TypeDef tx_obs_port, uint8_t tx_obs_pin)
{
  // Turn on the PRS and GPIO clocks to access their registers.
  // Configure pins as output.
  GPIO_PinModeSet(rx_obs_port, rx_obs_pin, gpioModePushPull, 0);
  GPIO_PinModeSet(tx_obs_port, tx_obs_pin, gpioModePushPull, 0);

#ifdef _SILICON_LABS_32B_SERIES_2


  // Configure PRS Channel 0 to output RAC_RX.
  PRS_SourceAsyncSignalSet(0,PRS_ASYNC_CH_CTRL_SOURCESEL_RAC,_PRS_ASYNC_CH_CTRL_SIGSEL_RACRX) ;
  PRS_PinOutput(RX_OBS_PRS_CHANNEL, prsTypeAsync,rx_obs_port, rx_obs_pin);

  /* Configure PRS Channel 1 to output RAC_RX.*/
  PRS_SourceAsyncSignalSet(1,PRS_ASYNC_CH_CTRL_SOURCESEL_RAC,_PRS_ASYNC_CH_CTRL_SIGSEL_RACTX) ;
  PRS_PinOutput(TX_OBS_PRS_CHANNEL, prsTypeAsync,tx_obs_port, tx_obs_pin);
#elif defined(_SILICON_LABS_32B_SERIES_1)
  if((tx_obs_port != gpioPortF)||(rx_obs_port != gpioPortF)){
        return;
  }
  if(rx_obs_port == gpioPortF){
      ch0_loc = rx_obs_pin;
  }

  /*set the radio RX active to channel 0*/
  PRS_SourceAsyncSignalSet(RX_OBS_PRS_CHANNEL,
         PRS_RAC_RX,
         ((PRS_RAC_RX & _PRS_CH_CTRL_SIGSEL_MASK)>> _PRS_CH_CTRL_SIGSEL_SHIFT));
  /* set the location for channel0 and enable it*/
  PRS_GpioOutputLocation(RX_OBS_PRS_CHANNEL,/*ch0_loc*/rx_obs_pin);

  /*set radio Tx active to channel 1.
   * channel 1. Locations can be 0 - 7*/
  PRS_SourceAsyncSignalSet(TX_OBS_PRS_CHANNEL,
         (PRS_RAC_RX & _PRS_CH_CTRL_SOURCESEL_MASK),
         ((PRS_RAC_RX & _PRS_CH_CTRL_SIGSEL_MASK)>> _PRS_CH_CTRL_SIGSEL_SHIFT));

   /*for series 1 PRS channel 1 can only be output on port F so the pin number minus 1 equates to the location*/
  PRS_GpioOutputLocation(TX_OBS_PRS_CHANNEL,tx_obs_pin-1);
#endif
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

  enableDebugGpios(gpioPortB,0,gpioPortB,1);

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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

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

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start general advertising and enable connections.
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
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Restart advertising after client has disconnected.
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
