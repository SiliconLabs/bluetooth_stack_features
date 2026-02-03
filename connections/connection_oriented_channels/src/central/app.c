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
#include "sl_simple_button_instances.h"

#define MAX_PDU_SIZE    200       // Max Protocol Data Unit size used by the L2CAP channel
#define MAX_SDU_SIZE    200       // Max Service Data Unit size used by the L2CAP channel
#define TX_PACKET_SIZE  200       // Size of the dummy packet to be sent
#define SPSM            0x81      // Protocol Service Multiplexor (PSM) value used by the L2CAP channel
#define INITIAL_CREDIT_REQUEST  50
#define TX_PACKET_DUMMY_VALUE 2   // Default value of the dummy packet to be sent

#define SIGNAL_BTN_PRESS      1

// The advertised name of the peripheral unit
static char dev_name[] = "CoC Peripheral";

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t connection = 0;
static uint16_t cid = 0;
static uint8_t tx_packet[TX_PACKET_SIZE] = { TX_PACKET_DUMMY_VALUE };
static uint16_t max_pdu = MAX_PDU_SIZE, max_sdu = MAX_SDU_SIZE, spsm = SPSM;
//Initial credit request. It will be overwritten with the value actually provided by the peripheral
static uint16_t credit = 0;
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
 * Parse advertisements looking for the name of the peripheral device
 * @param[in] data: Advertisement packet
 * @param[in] len:  Length of the advertisement packet
 *****************************************************************************/
static uint8_t find_name_in_advertisement(uint8_t *data, uint8_t len)
{
  uint8_t ad_field_length;
  uint8_t ad_field_type;
  uint8_t i = 0;

  // Parse advertisement packet
  while (i < len) {
    ad_field_length = data[i];
    ad_field_type = data[i + 1];
    // Shortened Local Name ($08) or Complete Local Name($08)
    if (ad_field_type == 0x08 || ad_field_type == 0x09) {
      // compare name
      if (memcmp(&data[i + 2], dev_name, (ad_field_length - 1)) == 0) {
        return 1;
      }
    }
    // advance to the next AD struct
    i = i + ad_field_length + 1;
  }
  return 0;
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
      app_log("CoC sample application, role: Central\r\n");
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);
      app_assert_status(sc);
      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      //Check scan response packet for the peripheral's name
      if (find_name_in_advertisement(evt->data.evt_scanner_legacy_advertisement_report.data.data,
                                     evt->data.evt_scanner_legacy_advertisement_report.data.len)) {
        sl_bt_scanner_stop();
        sc = sl_bt_connection_open(evt->data.evt_scanner_legacy_advertisement_report.address,
                                   evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                   sl_bt_gap_phy_1m, &connection);
        app_assert_status(sc);
      }
      break;
    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("Connection opened\r\n");
      sc = sl_bt_l2cap_open_le_channel(connection, spsm, max_sdu, max_pdu, INITIAL_CREDIT_REQUEST, &cid);
      app_assert_status(sc);
      break;

    case sl_bt_evt_l2cap_le_channel_open_response_id:
      if (!evt->data.evt_l2cap_le_channel_open_response.errorcode) {
        app_log("L2CAP open channel response received. cid: %x, credit: %d\r\n",
                evt->data.evt_l2cap_le_channel_open_response.cid,
                evt->data.evt_l2cap_le_channel_open_response.credit);
        credit = evt->data.evt_l2cap_le_channel_open_response.credit;
        cid    = evt->data.evt_l2cap_le_channel_open_response.cid;
      } else {
        app_log("Channel open request failed with %x\r\n", evt->data.evt_l2cap_le_channel_open_response.errorcode);
      }
      break;

    case sl_bt_evt_l2cap_channel_credit_id:
      credit += evt->data.evt_l2cap_channel_credit.credit;
      app_log("Received %d credits. Remaining: %d\r\n", evt->data.evt_l2cap_channel_credit.credit, credit);

      break;

    case sl_bt_evt_l2cap_command_rejected_id:
      app_log("Command rejected, code: %x, reason: %x\r\n", evt->data.evt_l2cap_command_rejected.code,
              evt->data.evt_l2cap_command_rejected.reason);
      break;

    case sl_bt_evt_l2cap_channel_closed_id:
      app_log("Channel closed, reason: %x", evt->data.evt_l2cap_channel_closed.reason);
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals == SIGNAL_BTN_PRESS) {
        if (credit > 0) {
          while (credit > 0) {
            sc = sl_bt_l2cap_channel_send_data(connection, cid, sizeof(tx_packet), tx_packet);
            if (!sc) {
              credit--;
              app_log("Remaining credits: %d\r\n", credit);
            } else {
              app_log("Error while sending packet: %x\r\n", sc);
            }
          }
        } else {
          app_log("No credits available\r\n");
        }
      }
      break;
    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
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

void sl_button_on_change(const sl_button_t *handle)
{
  sl_button_state_t state;
  if (handle->context == sl_button_btn0.context) {
    state = sl_button_get_state(&sl_button_btn0);
    if (state == SL_SIMPLE_BUTTON_PRESSED) {
      sl_bt_external_signal(SIGNAL_BTN_PRESS);
    }
  }
}
