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
#include "sl_simple_button_instances.h"

#define BTN0_IRQ_EVENT  0x1
#define BTN1_IRQ_EVENT  0x2

static uint8_t find_service_in_advertisement(uint8_t *data,
                                             uint8_t len);
static uint16_t queue_characteristic_chunk(uint8_t * data,
                                           uint16_t len,
                                           uint8_t connection,
                                           uint16_t characteristic ,
                                           uint16_t offset);
static uint16_t write_characteristic(uint8_t *data,
                                     uint16_t len,
                                     uint8_t connection,
                                     uint16_t characteristic,
                                     uint16_t offset);
static void set_mode(void);

typedef enum {
  IDLE,
  SCANNING,
  CONNECTED,
  DISCOVERING_SERVICES,
  DISCOVERING_CHARACTERISTICS,
  READING_CHARACTERISTIC,
  PREPARING_LONG_WRITE,
  EXECUTING_LONG_WRITE
} gatt_state_t;

static uint16_t att_mtu = 0;

static volatile bool is_central = false;
static uint8_t long_data_buffer[512];
static uint8_t test_data_to_write[512] = {0xff, 0xfe};

static const uint8_t advertised_service_uuid[16] = { 0x77,
                                                     0x23,
                                                     0x18,
                                                     0x63,
                                                     0x92,
                                                     0xc4,
                                                     0xf5,
                                                     0x87,
                                                     0x02,
                                                     0x4b,
                                                     0x16,
                                                     0xd7,
                                                     0x3c,
                                                     0x43,
                                                     0xb5,
                                                     0xcd};
static gatt_state_t gatt_state;
static uint8_t connection_handle = 0xff;
static uint32_t service_handle = 0xffffffff;
static uint16_t characteristic_handle = 0xffff;
static uint16_t write_offset = 0;

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Fill test data buffer
  for (uint16_t i = 0; i < sizeof(long_data_buffer); i++) {
    long_data_buffer[i] = i;
  }
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

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      /* Print stack version */
      app_log("Bluetooth stack booted: v%d.%d.%d-b%d\n",
                 evt->data.evt_system_boot.major,
                 evt->data.evt_system_boot.minor,
                 evt->data.evt_system_boot.patch,
                 evt->data.evt_system_boot.build);
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to get Bluetooth address\n",
                    (int)sc);

      /* Print Bluetooth address */
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
      app_log("boot event - starting advertising\r\n");
      app_log("Press PB0 to enter central mode\r\n");
      gatt_state = IDLE;
      break;

    // -------------------------------
    // This event is generated when an advertisement packet or a scan response
    // is received from a peripheral
    case sl_bt_evt_scanner_scan_report_id:
      // Parse advertisement packets
      if (evt->data.evt_scanner_scan_report.packet_type == 0) {
        // If a advertisement is found...
        if (find_service_in_advertisement(&(evt->data.evt_scanner_scan_report.data.data[0]),
                                          evt->data.evt_scanner_scan_report.data.len) != 0) {
          // then stop scanning for a while
          sc = sl_bt_scanner_stop();
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to stop discovery\n",
                        (int)sc);
          // and connect to that device
          sc = sl_bt_connection_open(evt->data.evt_scanner_scan_report.address,
                                     evt->data.evt_scanner_scan_report.address_type,
                                     gap_1m_phy,
                                     &connection_handle);
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to connect\n",
                        (int)sc);
          gatt_state = CONNECTED;
        }
      }
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log("connection opened\r\n");
      connection_handle = evt->data.evt_connection_opened.connection;
      if (is_central) {
        // Discover the service on the peripheral device
        sc = sl_bt_gatt_discover_primary_services(connection_handle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to discover primary services\n",
                      (int)sc);
        gatt_state = DISCOVERING_SERVICES;
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log("connection closed, reason: 0x%2.2x\r\n",
                 evt->data.evt_connection_closed.reason);
      connection_handle = 0xff;
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

    // -------------------------------
    // This event indicates that the external signals have been received
    case sl_bt_evt_system_external_signal_id:
      if (BTN0_IRQ_EVENT & evt->data.evt_system_external_signal.extsignals) {
        is_central ^= 1;
        set_mode();
      } 
      else if (BTN1_IRQ_EVENT & evt->data.evt_system_external_signal.extsignals) {
        app_log("test data write.\r\n");
        if ((connection_handle > 0) && (characteristic_handle == gattdb_long_data)) {
          write_offset = write_characteristic(test_data_to_write,
                                              sizeof(test_data_to_write),
                                              connection_handle,
                                              characteristic_handle,
                                              write_offset);
        }
      }
      break;

    // -------------------------------
    //  Indicates that an ATT_MTU exchange procedure is completed
    case sl_bt_evt_gatt_mtu_exchanged_id:
      att_mtu = evt->data.evt_gatt_mtu_exchanged.mtu;
      break;

    // -------------------------------
    // This event is generated when a new service is discovered
    case sl_bt_evt_gatt_service_id:
      if ((memcmp(evt->data.evt_gatt_service.uuid.data,
                      advertised_service_uuid,
                      evt->data.evt_gatt_service.uuid.len) == 0) && is_central) {
        app_log("Service found\r\n");
        // Save service handle for future reference
        service_handle = evt->data.evt_gatt_service.service;
      }
      break;

    // -------------------------------
    // This event is generated when a new characteristic is discovered
    case sl_bt_evt_gatt_characteristic_id:
      if ((evt->data.evt_gatt_characteristic.characteristic == gattdb_long_data)
        && is_central) {
        app_log("Characteristic found\r\n");
        // Save characteristic handle for future reference
        characteristic_handle = evt->data.evt_gatt_characteristic.characteristic;
      }
      break;

    // -------------------------------
    // This event is generated for various procedure completions, e.g. when a
    // write procedure is completed, or service discovery is completed
    case sl_bt_evt_gatt_procedure_completed_id:
      if (is_central == false) {
        break;
      }
      // If service discovery finished
      if (gatt_state == DISCOVERING_SERVICES) {
        // Discover characteristic on the peripheral device
        sc = sl_bt_gatt_discover_characteristics(connection_handle,
                                                  service_handle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to discover characteristics\n",
                      (int)sc);
        gatt_state = DISCOVERING_CHARACTERISTICS;
        break;
      }
      // If characteristic discovery finished
      if (gatt_state == DISCOVERING_CHARACTERISTICS) {
        app_log("Reading long characteristic value\r\n");
        sc = sl_bt_gatt_read_characteristic_value(connection_handle,
                                            characteristic_handle);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to reading long characteristic value\n",
                      (int)sc);
        gatt_state = READING_CHARACTERISTIC;
        break;
      }
      // If reading characteristic finished
      if (gatt_state == READING_CHARACTERISTIC) {
        app_log("Characteristic read finished with result %2X\r\n",
                   evt->data.evt_gatt_procedure_completed.result);
        gatt_state = CONNECTED;
        app_log("Connected. Press PB1 on central to write test data\r\n");
        break;
      }
      // If writing to the queue finished
      if (gatt_state == PREPARING_LONG_WRITE) {
        if (write_offset == (sizeof(test_data_to_write) - 1)) {
          gatt_state = EXECUTING_LONG_WRITE;
        } 
        else {
          write_offset += queue_characteristic_chunk(&test_data_to_write[write_offset],
                                                     sizeof(test_data_to_write) - write_offset,
                                                     connection_handle,
                                                     characteristic_handle,
                                                     write_offset);
        }
        break;
      }

      if (gatt_state == EXECUTING_LONG_WRITE) {
        /* Procedure completed in this state means
         * that the execute write is done,
         * so go to idle mode */
        gatt_state = IDLE;
        break;
      }
      break;

     // -------------------------------
     // This event is generated when a characteristic value was received
    case sl_bt_evt_gatt_characteristic_value_id:
      app_log("Received %d bytes on characteristic handle %d \r\n",
                evt->data.evt_gatt_characteristic_value.value.len,
                evt->data.evt_gatt_characteristic_value.characteristic);
      app_log("Starting from offset %d\r\n",
                evt->data.evt_gatt_characteristic_value.offset);
      break;

     // -------------------------------
    case sl_bt_evt_gatt_server_user_read_request_id:
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_long_data) {
        uint8_t offset = evt->data.evt_gatt_server_user_read_request.offset;
        static uint16_t bytes_left_to_send = sizeof(long_data_buffer);
        uint8_t bytes_in_this_tx = 0;
        uint16_t sent_len;

        if (bytes_left_to_send > att_mtu - 1) {
          bytes_in_this_tx = att_mtu - 1;
          bytes_left_to_send -= bytes_in_this_tx;
        } 
        else {
          bytes_in_this_tx = bytes_left_to_send;
          bytes_left_to_send = 0;     // reset for next time
        }

        sc = sl_bt_gatt_server_send_user_read_response(
              evt->data.evt_gatt_server_user_read_request.connection,
              evt->data.evt_gatt_server_user_read_request.characteristic,
              (uint8_t)SL_STATUS_OK,
              bytes_in_this_tx,
              &long_data_buffer[offset],
              &sent_len);
        app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to send a response\n",
                      (int)sc);
      }
      break;

    case sl_bt_evt_gatt_server_user_write_request_id:
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_long_data) {
          app_log_info("gatt_write_request opcode %2X, %d bytes at offset %d\r\n",
                     evt->data.evt_gatt_server_user_write_request.att_opcode,
                     evt->data.evt_gatt_server_user_write_request.value.len,
                     evt->data.evt_gatt_server_user_write_request.offset);
          memcpy(&long_data_buffer[evt->data.evt_gatt_server_user_write_request.offset],
                 evt->data.evt_gatt_server_user_write_request.value.data,
                 evt->data.evt_gatt_server_user_write_request.value.len);

          if(evt->data.evt_gatt_server_user_write_request.att_opcode == sl_bt_gatt_write_request ){
          sc = sl_bt_gatt_server_send_user_write_response(
              evt->data.evt_gatt_server_user_write_request.connection,
              evt->data.evt_gatt_server_user_write_request.characteristic,
              (uint8_t)SL_STATUS_OK);
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to send a write response\n",
                        (int)sc);
          } else if (evt->data.evt_gatt_server_user_write_request.att_opcode == sl_bt_gatt_prepare_write_request){
          sc = sl_bt_gatt_server_send_user_prepare_write_response(
              evt->data.evt_gatt_server_user_write_request.connection,
              evt->data.evt_gatt_server_user_write_request.characteristic,
              (uint8_t)SL_STATUS_OK,
              evt->data.evt_gatt_server_user_write_request.offset,
              evt->data.evt_gatt_server_user_write_request.value.len,
              evt->data.evt_gatt_server_user_write_request.value.data);
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to send a prepare write response\n",
                        (int)sc);
          }
      }
      if(evt->data.evt_gatt_server_user_write_request.att_opcode == sl_bt_gatt_execute_write_request){
          sc = sl_bt_gatt_server_send_user_write_response(evt->data.evt_gatt_server_user_write_request.connection,
                                                     evt->data.evt_gatt_server_user_write_request.characteristic,
                                                     (uint8_t)SL_STATUS_OK);
          app_assert(sc == SL_STATUS_OK,
                        "[E: 0x%04x] Failed to send a write response\n",
                        (int)sc);
      }
      break;

    case sl_bt_evt_gatt_server_execute_write_completed_id:
      app_log("Queued GATT write completed ");
      app_log("result = 0x%2X\r\n",
                 evt->data.evt_gatt_server_execute_write_completed.result);
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

/**************************************************************************//**
 * Simple Button
 * Button state changed callback
 * @param[in] handle Button event handle
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  // Button pressed.
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&sl_button_btn0 == handle) {
      sl_bt_external_signal(BTN0_IRQ_EVENT);
    }
    else if (&sl_button_btn1 == handle) {
      sl_bt_external_signal(BTN1_IRQ_EVENT);
    }
  }
}

// Parse advertisements looking for advertised service
static uint8_t find_service_in_advertisement(uint8_t *data, uint8_t len)
{
  uint8_t ad_field_length;
  uint8_t ad_field_type;
  uint8_t i = 0;
  // Parse advertisement packet
  while (i < len) {
    ad_field_length = data[i];
    ad_field_type = data[i + 1];
    // Incomplete ($06) or Complete ($07) list of 128-bit UUIDs
    if (ad_field_type == 0x06 || ad_field_type == 0x07) {
      // compare UUID to advertised_service_uuid
      if (memcmp(&data[i + 2], advertised_service_uuid, 16) == 0) {
        return 1;
      }
    }
    // advance to the next AD struct
    i = i + ad_field_length + 1;
  }
  return 0;
}

/*
 * @return number of bytes queued up
 */
static uint16_t queue_characteristic_chunk(uint8_t * data,
                                           uint16_t len,
                                           uint8_t connection,
                                           uint16_t characteristic ,
                                           uint16_t offset)
{
  uint8_t max_tx_size = att_mtu - 5;
  uint16_t sent_len;
  sl_status_t sc;

  if (len > max_tx_size) {
    sc = sl_bt_gatt_prepare_characteristic_value_write(connection,
                                                       characteristic,
                                                       offset,
                                                       max_tx_size,
                                                       data,
                                                       &sent_len);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to prepare write\n",
                  (int)sc);
    app_log("Wrote %d bytes to remote\r\n", sent_len);
    return sent_len;
  } 
  else if (len) {
    /* check to see if there is any data smaller than max_tx_size to be sent*/
    sc = sl_bt_gatt_prepare_characteristic_value_write(connection,
                                                       characteristic,
                                                       offset,
                                                       len,
                                                       data,
                                                       &sent_len);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to prepare write\n",
                  (int)sc);
    app_log("Wrote %d bytes to remote\r\n", sent_len);
    return sent_len;
  } 
  else { /* len == 0*/
    /* part 3 executing the write*/
    /* send all the queued blocks */
    gatt_state = EXECUTING_LONG_WRITE;
    sc = sl_bt_gatt_execute_characteristic_value_write(connection,
                                                       (uint8_t)gatt_commit);
    app_log("exec_result = 0x%X\r\n", sc);
  }

  return 0;
}

static uint16_t write_characteristic(uint8_t *data,
                                     uint16_t len,
                                     uint8_t connection,
                                     uint16_t characteristic,
                                     uint16_t offset)
{
  /* Need to split this function into three parts
   * so that the gatt_procedure_completed event can handle it. */
  /* part 1 initiate the process */
  if (gatt_state == CONNECTED) {
    gatt_state = PREPARING_LONG_WRITE;
    return queue_characteristic_chunk(data,
                                      len,
                                      connection,
                                      characteristic,
                                      offset);
  } 
  else {
    app_log("GATT busy, please try again later\r\n");
    return -1;
  }
  return SL_STATUS_OK;
}

/*
 * Helper function to put the device in central/peripheral mode
 * and start scanning or advertising accordingly.
*/
static void set_mode(void)
{
  sl_status_t sc;

  app_log("Entering %s mode\r\n", (is_central) ? "central" : "peripheral");

  if (is_central) {
    if (gatt_state == SCANNING) {
      sc = sl_bt_scanner_stop();
      app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to stop scanning\n",
                    (int)sc);
    }
    // Set passive scanning on 1Mb PHY
    sc = sl_bt_scanner_set_mode(gap_1m_phy, 0);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set discovery type\n",
                  (int)sc);
    // Set scan interval and scan window
    sc = sl_bt_scanner_set_timing(gap_1m_phy, 160, 160);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set discovery timing\n",
                  (int)sc);
    // Start scanning - looking for peripheral devices
    sc = sl_bt_scanner_start(gap_1m_phy, scanner_discover_observation);
    app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start discovery #1\n",
                  (int)sc);
    gatt_state = SCANNING;
    app_log("starting scanning\r\n");
  } else {

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
    app_log("boot event - starting advertising\r\n");
  }
}
