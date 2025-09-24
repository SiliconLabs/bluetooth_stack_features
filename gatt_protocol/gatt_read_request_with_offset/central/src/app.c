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
#include "app.h"
#include "app_timer.h"

#include "app_button_press.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

typedef enum {
  IDLE,
  CONNECTING,
  DISCOVERING_SERVICES,
  DISCOVERING_CHARACTERISTICS,
  READING_CHARACTERISTIC,
  READING_CHARACTERISTIC_DONE
} gatt_state_t;

#define PAYLOAD_SIZE  330
#define MTU_SIZE  204

typedef struct {
  uint8_t connection;
  uint32_t service;
  gatt_state_t gatt_state;
  uint8_t payload[PAYLOAD_SIZE];
} node_t;

#define NO_CALLBACK_DATA  (void *)NULL
#define PERIPHERAL_COUNT  SL_BT_CONFIG_MAX_CONNECTIONS
#define BUTTON_0          0
#define APP_CHARACTERISTIC_READING_TIMER_TIMEOUT       100

static node_t empty_node = {0, 0, IDLE, {}};
static node_t nodes[PERIPHERAL_COUNT];

static char peripheral_name[]= "Demo Peripheral";
static app_timer_t app_characteristic_reading_timer;

static uint8_t find_name_in_advertisement(uint8_t *data, uint8_t len);
static void app_characteristic_reading_timer_cb(app_timer_t *handle, void *data);
static uint8_t get_peripheral_by_connection(uint8_t connection);

static const uint8_t service_uuid[2] = { 0xAA, 0xAA };
static const uint8_t characteristics_uuid[2] = { 0xBB, 0xBB };

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  app_log("Boot\r\n");

  // Enable button
  sl_simple_button_enable(&sl_button_btn0);
  // Wait
  sl_sleeptimer_delay_millisecond(1);
  // Enable button presses
  app_button_press_enable();

  for (uint8_t i = 0; i < PERIPHERAL_COUNT; i++) {
    nodes[i] = empty_node;
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
  sl_status_t sc;
  uint8_t index;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Reduce MTU to avoid package splitting
      sc = sl_bt_gatt_set_max_mtu(MTU_SIZE, NULL);

      sc = app_timer_start(&app_characteristic_reading_timer,
                           APP_CHARACTERISTIC_READING_TIMER_TIMEOUT,
                           app_characteristic_reading_timer_cb,
                           NO_CALLBACK_DATA,
                           true);
      app_assert_status(sc);

      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);
      app_assert_status(sc);

      break;

    case sl_bt_evt_gatt_mtu_exchanged_id:
      app_log("Exchanged Mtu: %d\r\n", evt->data.evt_gatt_mtu_exchanged.mtu);

      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      if(find_name_in_advertisement(evt->data.evt_scanner_legacy_advertisement_report.data.data,
                                    evt->data.evt_scanner_legacy_advertisement_report.data.len))
      {
        sl_bt_scanner_stop();

        index = get_peripheral_by_connection(0);

        sc = sl_bt_connection_open(evt->data.evt_scanner_legacy_advertisement_report.address,
                                   evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                   sl_bt_gap_phy_1m, &nodes[index].connection);
        app_assert_status(sc);
        nodes[index].gatt_state = CONNECTING;
      }

      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("Connected\r\n");

      index = get_peripheral_by_connection(evt->data.evt_connection_opened.connection);

      sc = sl_bt_gatt_discover_primary_services_by_uuid(nodes[index].connection, sizeof(service_uuid), service_uuid);
      app_assert_status(sc);
      nodes[index].gatt_state = DISCOVERING_SERVICES;
      break;

    case sl_bt_evt_gatt_service_id:
      app_log("Services\r\n");

      index = get_peripheral_by_connection(evt->data.evt_gatt_service.connection);

      nodes[index].service = evt->data.evt_gatt_service.service;
      //app_log("UUID: %X %X\r\n", evt->data.evt_gatt_service.uuid.data[0], evt->data.evt_gatt_service.uuid.data[1]);
      break;

    case sl_bt_evt_gatt_procedure_completed_id:
      //app_log("Procedure: %X\r\n", evt->data.evt_gatt_procedure_completed.result);

      index = get_peripheral_by_connection(evt->data.evt_gatt_procedure_completed.connection);

      switch(nodes[index].gatt_state) {
        case DISCOVERING_SERVICES: {
          app_log("Service Request: %ld\r\n", nodes[index].service);
          sc = sl_bt_gatt_discover_characteristics_by_uuid(nodes[index].connection, nodes[index].service, sizeof(characteristics_uuid), characteristics_uuid);
          app_assert_status(sc);
          nodes[index].gatt_state = DISCOVERING_CHARACTERISTICS;
        }; break;
        case DISCOVERING_CHARACTERISTICS: {
          sc = sl_bt_gatt_read_characteristic_value_by_uuid(nodes[index].connection, nodes[index].service, sizeof(characteristics_uuid), characteristics_uuid);
          app_assert_status(sc);
          nodes[index].gatt_state = READING_CHARACTERISTIC;
        }; break;
        case READING_CHARACTERISTIC: {
          //app_log("READING_CHARACTERISTIC_DONE\r\n");
          nodes[index].gatt_state = READING_CHARACTERISTIC_DONE;

          app_log("Node Read: %d\r\n", index);
          /*for (uint16_t i = 0; i < PAYLOAD_SIZE; i++) {
            app_log("%X", nodes[index].payload[i]);
          }
          app_log("\r\n");*/

        }; break;
        default: break;
      }
      break;

    case sl_bt_evt_gatt_characteristic_id:
      app_log("Characteristics\r\n");
      break;

    case sl_bt_evt_gatt_characteristic_value_id:
      //app_log("Offset %d\r\n", evt->data.evt_gatt_characteristic_value.offset);
      //app_log("Length %d\r\n", evt->data.evt_gatt_characteristic_value.value.len);
      //app_log("Characteristic Value: %X\r\n", evt->data.evt_gatt_characteristic_value.value.data[0]);

      index = get_peripheral_by_connection(evt->data.evt_gatt_characteristic_value.connection);

      memcpy(&nodes[index].payload[evt->data.evt_gatt_characteristic_value.offset],
             evt->data.evt_gatt_characteristic_value.value.data,
             evt->data.evt_gatt_characteristic_value.value.len);


      //app_log("Characteristic Value: %X %X\r\n", nodes[index].payload[evt->data.evt_gatt_characteristic_value.offset], nodes[index].payload[evt->data.evt_gatt_characteristic_value.value.len - 1]);

      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log("Disconnected: %d\r\n", evt->data.evt_connection_closed.connection);

      index = get_peripheral_by_connection(evt->data.evt_connection_closed.connection);
      nodes[index].connection = 0;
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      app_log_debug("Unhandled event [0x%08lx]\r\n",
                    SL_BT_MSG_ID(evt->header));
      break;
  }
}

static uint8_t find_name_in_advertisement(uint8_t *data, uint8_t len)
{
  uint8_t ad_field_length;
  uint8_t ad_field_type;
  uint8_t i = 0;

  // Parse advertisement packet
  while (i < len) {
    ad_field_length = data[i];
    ad_field_type = data[i + 1];
    // Shortened Local Name ($08) or Complete Local Name($09)
    if (ad_field_type == 0x08 || ad_field_type == 0x09) {
      // compare name
      app_log("Name: ");
      for(uint8_t name_index = i + 2; name_index < i + ad_field_length + 1; name_index++) {
        app_log("%c", data[name_index]);
      }
      app_log("\r\n");
      if (memcmp(&data[i + 2], peripheral_name, (ad_field_length - 1)) == 0) {
        app_log("Found\r\n");
        return 1;
      }
    }
    // advance to the next AD struct
    i = i + ad_field_length + 1;
  }
  return 0;
}

/***************************************************************************//**
 * Timer Callbacks
 ******************************************************************************/
static void app_characteristic_reading_timer_cb(app_timer_t *handle, void *data)
{
  (void)data;
  (void)handle;
  sl_status_t sc;

  for(uint8_t index = 0; index < PERIPHERAL_COUNT; index++) {
    if(nodes[index].gatt_state == READING_CHARACTERISTIC_DONE) {
      sc = sl_bt_gatt_read_characteristic_value_by_uuid(nodes[index].connection, nodes[index].service, sizeof(characteristics_uuid), characteristics_uuid);
      app_assert_status(sc);
      nodes[index].gatt_state = READING_CHARACTERISTIC;
    }
  }
}


void app_button_press_cb(uint8_t button, uint8_t duration)
{
  sl_status_t sc;
  // Selecting action by duration
  switch (duration) {
    case APP_BUTTON_PRESS_DURATION_LONG:
      // Handling of button press greater than 1s and less than 5s
      if (button == BUTTON_0) {

        uint8_t index = get_peripheral_by_connection(0);

        if(index == 6) {
          app_log("Peripherals pool exhausted!\r\n");
          return;
        }

        sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);
        app_assert_status(sc);
      }
      break;
    default:
      break;
  }
}

static uint8_t get_peripheral_by_connection(uint8_t connection)
{
  uint8_t index = 0;
  while (index < PERIPHERAL_COUNT) {
    if(nodes[index].connection == connection) break;
    index++;
  }
  return index;
}
