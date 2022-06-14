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

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static uint8_t ext_adv = 0;

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
      demo_setup_adv(advertising_set_handle);
      // Start general advertising and enable connections.
      if(ext_adv) sc = sl_bt_extended_advertiser_start(
        advertising_set_handle,
        sl_bt_extended_advertiser_connectable,
        0);
      else sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_legacy_advertiser_connectable);
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
      if(ext_adv) sc = sl_bt_extended_advertiser_start(
        advertising_set_handle,
        sl_bt_extended_advertiser_connectable,
        0);
      else sc = sl_bt_legacy_advertiser_start(
        advertising_set_handle,
        sl_bt_legacy_advertiser_connectable);
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

/******************************************************************
 * Advertisement constructor
 * ***************************************************************/
void demo_setup_adv(uint8_t handle)
{
  sl_status_t sc;
  const uint8_t flag_data = 0x6;
  const uint8_t local_name_data[] = "AdvC";

  /* https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers - To get your company ID*/
  /* Below is an example to construct your manufacturer specific data with payload set to "KBA - Adv Constructor" */
  uint16_t company_id = 0x02FF; // 0x02FF - Silicon Labs' company ID
  uint8_t manu_data[sizeof("KBA - Adv Constructor") + 1];
  memcpy(manu_data, (uint8_t *)&company_id, 2);
  memcpy(manu_data + 2, (uint8_t *)("KBA - Adv Constructor"), sizeof("KBA - Adv Constructor") - 1);

  ad_element_t ad_elements[] = {
    /* Element 0 */
    {
      .ad_type = flags,
      .len = 1,
      .data = &flag_data
    },
    /* Element 1 */
    {
      .ad_type = complete_local_name,
      .len = sizeof(local_name_data) - 1,
      .data = local_name_data
    },
    /* Element 2 */
    {
      .ad_type = manufacturer_specific_data,
      .len = sizeof(manu_data),
      .data = manu_data
    }
  };

  /* Set up advertisement payload with the first 2 elements */
  adv_t adv = {
    .adv_handle = handle,
    .adv_packet_type = adv_packet,
    .ele_num = 2,
    .p_element = ad_elements
  };
  sc = construct_adv(&adv, ext_adv);
  if (sc != SL_STATUS_OK) {
      app_log("Check error here [%s:%u]\n", __FILE__, __LINE__);
  }

  /* Set up scan response payload with the last (3th) element */
  adv.adv_handle = handle;
  adv.adv_packet_type = scan_rsp;
  adv.ele_num = 1;
  adv.p_element = &ad_elements[2];

  sc = construct_adv(&adv, ext_adv);
  if (sc != SL_STATUS_OK) {
    app_log("Check error here [%s:%u]\n", __FILE__, __LINE__);
  }
}

sl_status_t construct_adv(const adv_t *adv, uint8_t ext_adv)
{
  uint8_t amout_bytes = 0, i;
  uint8_t buf[MAX_EXTENDED_ADV_LENGTH] = { 0 };
  sl_status_t sc;

  if (!adv) {
      app_log("input param null, aborting.\n");
      return SL_STATUS_NULL_POINTER;
  }

  for (i = 0; i < adv->ele_num; i++) {
    amout_bytes += adv->p_element[i].len + 2;
    if (!adv->p_element[i].data) {
        app_log("adv unit payload data null, aborting.\n");
        return SL_STATUS_NULL_POINTER;
    }
  }
  if (((amout_bytes > MAX_ADV_DATA_LENGTH) && !ext_adv)
      || ((amout_bytes > MAX_EXTENDED_ADV_LENGTH))) {
      app_log("Adv data too long [length = %d], aborting.\n", amout_bytes);
      return SL_STATUS_BT_CTRL_PACKET_TOO_LONG;
  }

  amout_bytes = 0;
  for (i = 0; i < adv->ele_num; i++) {
    buf[amout_bytes++] = adv->p_element[i].len + 1;
    buf[amout_bytes++] = adv->p_element[i].ad_type;
    memcpy(buf + amout_bytes, adv->p_element[i].data, adv->p_element[i].len);
    amout_bytes += adv->p_element[i].len;
  }
  if(ext_adv) sc = sl_bt_extended_advertiser_set_data(adv->adv_handle, amout_bytes, buf);
  else sc = sl_bt_legacy_advertiser_set_data(adv->adv_handle, adv->adv_packet_type, amout_bytes, buf);
  app_assert(sc == SL_STATUS_OK,
                    "[E: 0x%04x] Failed to set advertising data\n",
                    (int)sc);
  return SL_STATUS_OK;
}
