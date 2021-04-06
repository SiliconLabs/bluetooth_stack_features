/***************************************************************************/ /**
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
#include "sl_app_log.h"

/** Timer Frequency used. */
#define TIMER_CLK_FREQ ((uint32_t)32768)
/** Convert msec to timer ticks. */
#define TIMER_MS_2_TIMERTICK(ms) ((TIMER_CLK_FREQ * ms) / 1000)
/** Stop timer. */
#define TIMER_STOP  0

#define LEGACY_PAIRING            1
#define SECURE_CONNECTION_PAIRING 2

#define PAIRING_MODE SECURE_CONNECTION_PAIRING

#define DISCONNECTED 0
#define SCANNING     1
#define FIND_SERVICE 2
#define FIND_CHAR    3
#define ENABLE_NOTIF 4
#define DATA_MODE    5

#define WRITE_TIMER 1

const uint8_t Svc_uuid[] = {
    0x07,
    0xb9,
    0xf9,
    0xd7,
    0x50,
    0xa1,
    0x20,
    0x89,
    0x77,
    0x40,
    0xcb,
    0xfd,
    0x2c,
    0xc1,
    0x80,
    0x48};
const uint8_t Ntf_uuid[] = {
    0x07,
    0xb9,
    0xf9,
    0xd7,
    0x50,
    0xa2,
    0x20,
    0x89,
    0x77,
    0x40,
    0xcb,
    0xfd,
    0x2c,
    0xc1,
    0x80,
    0x48};
const uint8_t Wrt_uuid[] = {
    0x07,
    0xb9,
    0xf9,
    0xd7,
    0x50,
    0xa3,
    0x20,
    0x89,
    0x77,
    0x40,
    0xcb,
    0xfd,
    0x2c,
    0xc1,
    0x80,
    0x48};

#define PRINT_GATT_INFO

static uint8_t _conn_handle = 0xFF;
static uint8_t _main_state;
static uint32_t _service_handle;
static uint16_t ntf_char_handle;
static uint16_t wrt_char_handle;
static uint8_t writeBuf[21] = {0};
#if (PAIRING_MODE == LEGACY_PAIRING)
static uint8_t oobData[16];
#else
static uint8_t oobData[32];
#endif

void Reset_variables()
{
  _conn_handle = 0xFF;
  _main_state = DISCONNECTED;
  _service_handle = 0;
  ntf_char_handle = 0;
  wrt_char_handle = 0;
  memset(writeBuf, '0', 20);
}

uint8_t Process_scan_response(sl_bt_evt_scanner_scan_report_t *pResp)
{
  // decoding advertising packets is done here. The list of AD types can be found
  // at: https://www.bluetooth.com/specifications/assigned-numbers/Generic-Access-Profile

  uint8_t i = 0;
  uint8_t ad_match_found = 0;
  uint8_t ad_len;
  uint8_t ad_type;

  while (i < (pResp->data.len - 1))
  {

    ad_len = pResp->data.data[i];
    ad_type = pResp->data.data[i + 1];

    if (ad_type == 0x06 || ad_type == 0x07)
    {
      // type 0x06 = More 128-bit UUIDs available
      // type 0x07 = Complete list of 128-bit UUIDs available

      // note: this check assumes that the service we are looking for is first
      // in the list. To be fixed so that there is no such limitation...
      if (memcmp(Svc_uuid, &(pResp->data.data[i + 2]), 16) == 0)
      {
        ad_match_found = 1;
        break;
      }
    }
    //jump to next AD record
    i = i + ad_len + 1;
  }
  return (ad_match_found);
}

static void writeToPeripheral()
{
  sl_status_t ret;

  uint8_t i = writeBuf[0];
  if (i == '9')
  {
    i = '0';
    memset(writeBuf, i, 20);
  }
  else
  {
    memset(writeBuf, i + 1, 20);
  }
  ret = sl_bt_gatt_write_characteristic_value(_conn_handle, wrt_char_handle, 20, writeBuf);
  sl_app_assert(ret == SL_STATUS_OK,
                "[E: 0x%04x] Write operation failed\n",
                (int)ret);
}
/**************************************************************************/ /**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************/ /**
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

/**************************************************************************/ /**
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

#if (PAIRING_MODE == LEGACY_PAIRING)
  uint32_t psData32 = 10;
#endif
  switch (SL_BT_MSG_ID(evt->header))
  {
  // -------------------------------
  // This event indicates the device has started and the radio is ready.
  // Do not call any stack command before receiving this boot event!
  case sl_bt_evt_system_boot_id:
  case sl_bt_evt_connection_closed_id:
    sl_app_log("stack version: %u.%u.%u\r\r\n", evt->data.evt_system_boot.major, evt->data.evt_system_boot.minor, evt->data.evt_system_boot.patch);
    sl_app_log("Central Boot\r\n");
    // Extract unique ID from BT Address.
    sc = sl_bt_system_get_identity_address(&address, &address_type);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to get Bluetooth address\n",
                  (int)sc);
    sl_app_log("local BT device address: ");
    for (uint8_t i = 0; i < 5; i++)
    {
      sl_app_log("%2.2x:", address.addr[5 - i]);
    }
    sl_app_log("%2.2x\r\n", address.addr[0]);
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

    /* delete all bondings to force the pairing process */
    sl_app_log("All bonding deleted\r\n");
    sc = sl_bt_sm_delete_bondings();
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to delete bondings\r\n",
                  (int)sc);

    sl_bt_sm_configure(0x0B, sm_io_capability_keyboarddisplay);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to configure security requirements and I/O capabilities of the system\r\n",
                  (int)sc);

    sl_bt_sm_set_bondable_mode(1);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set the device accept new bondings\r\n",
                  (int)sc);

#if (PAIRING_MODE == SECURE_CONNECTION_PAIRING)
    sl_app_log("Use Secure Connection Pairing.\r\n");
    sc = sl_bt_sm_use_sc_oob(1, sizeof(oobData), NULL, (uint8_t *)&oobData);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to enable the use of OOB data\r\n",
                  (int)sc);

    sl_app_log("Enable OOB with Secure Connection Pairing - Success.\r\n");
    sl_app_log("16-byte OOB data: ");
    for (uint8_t i = 0; i < 16; i++)
    {
      sl_app_log("0x%02X ", oobData[i]);
    }
    sl_app_log("\r\n");
    sl_app_log("16-byte confirm value: ");
    for (uint8_t i = 16; i < 32; i++)
    {
      sl_app_log("0x%02X ", oobData[i]);
    }
    sl_app_log("\r\n");
#elif (PAIRING_MODE == LEGACY_PAIRING)
    sl_app_log("Use Legacy Pairing.\r\n");
    uint32_t value_data;
    size_t value_len;
    sc = sl_bt_nvm_load(0x4000, 4, &value_len, (uint8_t *)&value_data);
    sl_app_assert(sc == SL_STATUS_OK || sc == SL_STATUS_BT_PS_KEY_NOT_FOUND,
                  "[E: 0x%04x] Failed to retrieve the value of the specified NVM key\r\n",
                  (int)sc);

    // First time
    if (value_len == 0)
    {
      sc = sl_bt_nvm_save(0x4000, 4, (uint8_t *)&psData32);
    }
    else
    {
      memcpy((uint8_t *)&psData32, (uint8_t *)&value_data, 4);
      psData32++;
      sc = sl_bt_nvm_save(0x4000, 4, (uint8_t *)&psData32);
    }
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to store a value into the specified NVM key\r\n",
                  (int)sc);

    sl_app_log("16-byte array PIN: ");
    srand(psData32);
    for (uint8_t i = 0; i < 16; i++)
    {
      oobData[i] = rand() % 0xFF;
      sl_app_log("0x%02X ", oobData[i]);
    }
    sl_app_log("\r\n");
    sc = sl_bt_sm_set_oob_data(16, oobData);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to set OOB data\r\n",
                  (int)sc);
#endif
    Reset_variables();
    // start discovery
    sl_bt_scanner_start(0x01, sl_bt_scanner_discover_generic);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start scanning for advertising devices\r\n",
                  (int)sc);
    break;
  case sl_bt_evt_scanner_scan_report_id:
    // process scan responses: this function returns 1 if we found the service we are looking for
    if (Process_scan_response(&(evt->data.evt_scanner_scan_report)) > 0)
    {
      uint8_t pResp_connection;

      // match found -> stop discovery and try to connect
      sc = sl_bt_scanner_stop();
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to stop scanning for advertising devices\r\n",
                    (int)sc);

      sc = sl_bt_connection_open(evt->data.evt_scanner_scan_report.address, evt->data.evt_scanner_scan_report.address_type, 0x01, &pResp_connection);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to connect to an advertising device\r\n",
                    (int)sc);

      // make copy of connection handle for later use (for example, to cancel the connection attempt)
      _conn_handle = pResp_connection;
    }
    break;
  // -------------------------------
  // This event indicates that a new connection was opened.
  case sl_bt_evt_connection_opened_id:
    // Start service discovery (we are only interested in one UUID)
    sl_app_log("Connected\r\n");
    sc = sl_bt_gatt_discover_primary_services_by_uuid(_conn_handle, 16, Svc_uuid);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to start service discovery\r\n",
                  (int)sc);
    _main_state = FIND_SERVICE;
    break;

  case sl_bt_evt_gatt_service_id:
    if (evt->data.evt_gatt_service.uuid.len == 16)
    {
      if (memcmp(Svc_uuid, evt->data.evt_gatt_service.uuid.data, 16) == 0)
      {
        // Service Found.
        sl_app_log("Specified Service Found.\r\n");
        _service_handle = evt->data.evt_gatt_service.service;
      }
    }
    break;

  case sl_bt_evt_gatt_characteristic_id:
    if (evt->data.evt_gatt_characteristic.uuid.len == 16)
    {
      if (memcmp(Ntf_uuid, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0)
      {
        ntf_char_handle = evt->data.evt_gatt_characteristic.characteristic;
      }
      else if (memcmp(Wrt_uuid, evt->data.evt_gatt_characteristic.uuid.data, 16) == 0)
      {
        wrt_char_handle = evt->data.evt_gatt_characteristic.characteristic;
      }
    }
    break;

  case sl_bt_evt_gatt_procedure_completed_id:
    switch (_main_state)
    {
    case FIND_SERVICE:

      if (_service_handle != 0)
      {
        // Service found, next search for characteristics
        sc = sl_bt_gatt_discover_characteristics(_conn_handle, _service_handle);
        sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to discover all characteristics\r\n",
                      (int)sc);
        _main_state = FIND_CHAR;
      }
      else
      {
        // no service found -> disconnect
        sc = sl_bt_connection_close(_conn_handle);
        sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to disconnect\r\n",
                      (int)sc);
      }
      break;

    case FIND_CHAR:
      if (ntf_char_handle && wrt_char_handle)
      {
        // Char found, turn on indications
        sc = sl_bt_gatt_set_characteristic_notification(_conn_handle, ntf_char_handle, gatt_notification);
        sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to enable the notification\r\n",
                      (int)sc);
        _main_state = ENABLE_NOTIF;
      }
      else
      {
        // no characteristic found? -> disconnect
        sc = sl_bt_connection_close(_conn_handle);
        sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to disconnect\r\n",
                      (int)sc);
      }
      break;

    case ENABLE_NOTIF:
      _main_state = DATA_MODE;
      sc = sl_bt_system_set_soft_timer(TIMER_MS_2_TIMERTICK(3000), WRITE_TIMER, true);
      sl_app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set soft timer\n",
                    (int)sc);
      break;

    default:
      break;
    }
    break;

  case sl_bt_evt_gatt_characteristic_value_id:
    if ((evt->data.evt_gatt_characteristic_value.att_opcode == gatt_handle_value_notification) && (evt->data.evt_gatt_characteristic_value.characteristic == ntf_char_handle))
    {
      // TODO: data notification is received here
#ifdef PRINT_GATT_INFO
      sl_app_log("Gatt Notification Received: ");
      for (uint8_t i = 0; i < evt->data.evt_gatt_characteristic_value.value.len; i++)
      {
        sl_app_log("%c", evt->data.evt_gatt_characteristic_value.value.data[i]);
      }
      sl_app_log("\r\n");
#endif
      ntf_char_handle = ntf_char_handle;
    }
    break;

  case sl_bt_evt_system_soft_timer_id:
    /* Check which software timer handle is in question */
    switch (evt->data.evt_system_soft_timer.handle)
    {
    case WRITE_TIMER: /* App UI Timer (LEDs, Buttons) */
      if ((_conn_handle != 0xFF) && (wrt_char_handle != 0))
      {
        writeToPeripheral();
        sc = sl_bt_system_set_soft_timer(TIMER_MS_2_TIMERTICK(3000), WRITE_TIMER, true);
        sl_app_assert(sc == SL_STATUS_OK,
                      "[E: 0x%04x] Failed to set soft timer\n",
                      (int)sc);
      }
      break;

    default:
      break;
    }
    break;

  case sl_bt_evt_sm_confirm_bonding_id:
    sl_app_log("Bonding confirmed\r\n");
    sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
    sl_app_assert(sc == SL_STATUS_OK,
                  "[E: 0x%04x] Failed to confirm bonding requests\r\n",
                  (int)sc);
    break;

  case sl_bt_evt_sm_passkey_display_id:
    sl_app_log("Passkey display\r\n");
    //      sl_app_log("Enter this passkey on the tablet:\r\n%d\r\n",
    //          evt->data.evt_sm_passkey_display.passkey);
    break;

  case sl_bt_evt_sm_passkey_request_id:
    sl_app_log("Passkey request\r\n");
    //      sl_app_log("Enter the passkey you see on the tablet\r\n");
    break;

  case sl_bt_evt_sm_confirm_passkey_id:
    sl_app_log("Confirm passkey\r\n");
    //                  sl_app_log("Are you see the same passkey on the tablet: %d (y/n)?\r\n",evt->data.evt_sm_confirm_passkey.passkey);
    break;

  case sl_bt_evt_sm_bonded_id:
    /* The bonding/pairing was successful so set the flag to allow indications to proceed */
    sl_app_log("Bonding completed\r\n");
    break;

  case sl_bt_evt_sm_bonding_failed_id:
    /* If the attempt at bonding/pairing failed, clear the bonded flag and display the reason */
    sl_app_log("Bonding failed: ");
    switch (evt->data.evt_sm_bonding_failed.reason)
    {
    case SL_STATUS_BT_SMP_PASSKEY_ENTRY_FAILED:
      sl_app_log("The user input of passkey failed\r\n");
      break;

    case SL_STATUS_BT_SMP_OOB_NOT_AVAILABLE:
      sl_app_log("Out of Band data is not available for authentication\r\n");
      break;

    case SL_STATUS_BT_SMP_AUTHENTICATION_REQUIREMENTS:
      sl_app_log("The pairing procedure cannot be performed as authentication requirements cannot be met due to IO capabilities of one or both devices\r\n");
      break;

    case SL_STATUS_BT_SMP_CONFIRM_VALUE_FAILED:
      sl_app_log("The confirm value does not match the calculated compare value\r\n");
      break;

    case SL_STATUS_BT_SMP_PAIRING_NOT_SUPPORTED:
      sl_app_log("Pairing is not supported by the device\r\n");
      break;

    case SL_STATUS_BT_SMP_ENCRYPTION_KEY_SIZE:
      sl_app_log("The resultant encryption key size is insufficient for the security requirements of this device\r\n");
      break;

    case SL_STATUS_BT_SMP_COMMAND_NOT_SUPPORTED:
      sl_app_log("The SMP command received is not supported on this device\r\n");
      break;

    case SL_STATUS_BT_SMP_UNSPECIFIED_REASON:
      sl_app_log("Pairing failed due to an unspecified reason\r\n");
      break;

    case SL_STATUS_BT_SMP_REPEATED_ATTEMPTS:
      sl_app_log("Pairing or authentication procedure is disallowed because too little time has elapsed since last pairing request or security request\r\n");
      break;

    case SL_STATUS_BT_SMP_INVALID_PARAMETERS:
      sl_app_log("Invalid Parameters\r\n");
      break;

    case SL_STATUS_BT_SMP_DHKEY_CHECK_FAILED:
      sl_app_log("The bonding does not exist\r\n");
      break;

    case SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING:
      sl_app_log("Pairing failed because of missing PIN, or authentication failed because of missing Key\r\n");
      break;

    case SL_STATUS_TIMEOUT:
      sl_app_log("Operation timed out\r\n");
      break;

    default:
      sl_app_log("Unknown error: 0x%X\r\n", evt->data.evt_sm_bonding_failed.reason);
      break;
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
