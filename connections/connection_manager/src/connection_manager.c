/***************************************************************************//**
 * @file
 * @brief Connection Manager
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
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
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_bluetooth.h"
#include "connection_manager.h"

static connection_t connections[SL_BT_CONFIG_MAX_CONNECTIONS];

void sli_bt_cm_init(void)
{
  memset(&connections, 0x00, SL_BT_CONFIG_MAX_CONNECTIONS * sizeof(connection_t));
}

void sli_bt_cm_on_event(sl_bt_msg_t *evt)
{
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_connection_opened_id:
      sli_bt_cm_add_connection(&evt->data.evt_connection_opened);
    break;
    case sl_bt_evt_connection_parameters_id:
      sli_bt_cm_update_parameters(&evt->data.evt_connection_parameters);
    break;
    case sl_bt_evt_sm_bonded_id:
      sli_bt_cm_update_bonding(&evt->data.evt_sm_bonded);
    break;
    case sl_bt_evt_connection_closed_id:
      sli_bt_cm_remove_connection(&evt->data.evt_connection_closed);
    break;
  }
}

sl_status_t sli_bt_cm_add_connection(sl_bt_evt_connection_opened_t *evt_data)
{
  sl_status_t sc;
  connection_t *connection;
  sc = sl_bt_cm_get_connection_by_handle(0x00, &connection);

  if(sc != SL_STATUS_OK) return SL_STATUS_FULL;

  connection->address = evt_data->address;
  connection->address_type = evt_data->address_type;
  connection->master = evt_data->master;
  connection->bonding = evt_data->bonding;
  connection->advertiser = evt_data->advertiser;
  connection->handle = evt_data->connection;

  return SL_STATUS_OK;
}

sl_status_t sli_bt_cm_update_parameters(sl_bt_evt_connection_parameters_t *evt_data)
{
  sl_status_t sc;
  connection_t *connection;
  sc = sl_bt_cm_get_connection_by_handle(evt_data->connection, &connection);

  if(sc != SL_STATUS_OK) return sc;

  connection->interval = evt_data->interval;
  connection->latency = evt_data->latency;
  connection->timeout = evt_data->timeout;
  connection->security_mode = evt_data->security_mode;
  connection->txsize = evt_data->txsize;

  return SL_STATUS_OK;
}

sl_status_t sli_bt_cm_update_bonding(sl_bt_evt_sm_bonded_t *evt_data)
{
  sl_status_t sc;
  connection_t *connection;
  sc = sl_bt_cm_get_connection_by_handle(evt_data->connection, &connection);

  if(sc != SL_STATUS_OK) return sc;

  connection->bonding = evt_data->bonding;
  connection->security_mode = evt_data->security_mode;

  return SL_STATUS_OK;
}

sl_status_t sli_bt_cm_remove_connection(sl_bt_evt_connection_closed_t *evt_data)
{
  sl_status_t sc;
  connection_t *connection;
  sc = sl_bt_cm_get_connection_by_handle(evt_data->connection, &connection);

  if(sc != SL_STATUS_OK) return sc;

  connection->handle = 0x00;

  return SL_STATUS_OK;
}

sl_status_t sl_bt_cm_get_connection_handles(uint8_t *connection_handles, uint8_t *size)
{
  uint8_t handle_index = 0;
  for (uint8_t connection_index = 0; connection_index < SL_BT_CONFIG_MAX_CONNECTIONS; connection_index++) {
    if (connections[connection_index].handle != 0x00) {
      if(handle_index == *size) return SL_STATUS_WOULD_OVERFLOW;
      connection_handles[handle_index] = connections[connection_index].handle;
      handle_index++;
    }
  }

  (*size) = handle_index;
  return (*size == 0 ) ? SL_STATUS_EMPTY : SL_STATUS_OK;
}



sl_status_t sl_bt_cm_get_connection_by_handle(uint8_t connection_handle, connection_t **connection)
{
  for (uint8_t connection_index = 0; connection_index < SL_BT_CONFIG_MAX_CONNECTIONS; connection_index++) {
    if (connections[connection_index].handle == connection_handle) {
      *connection = &connections[connection_index];
      return SL_STATUS_OK;
    }
  }

  return SL_STATUS_NOT_FOUND;
}

sl_status_t sl_bt_cm_get_connection_by_address(bd_addr *address, connection_t **connection)
{
  for (uint8_t connection_index = 0; connection_index < SL_BT_CONFIG_MAX_CONNECTIONS; connection_index++) {
    if (0 == memcmp(address, &(connections[connection_index].address), sizeof(bd_addr))) {
      *connection = &connections[connection_index];
      return SL_STATUS_OK;
    }
  }

  return SL_STATUS_NOT_FOUND;
}

bool sl_bt_cm_is_connection_list_full(void)
{
  sl_status_t sc;
  connection_t *connection;
  sc = sl_bt_cm_get_connection_by_handle(0x00, &connection);

  return sc == SL_STATUS_NOT_FOUND;
}

uint8_t sl_bt_cm_get_leftover_space(void)
{
  uint8_t leftover = 0;
  for (uint8_t connection_index = 0; connection_index < SL_BT_CONFIG_MAX_CONNECTIONS; connection_index++) {
    if (connections[connection_index].handle == 0x00) {
      leftover++;
    }
  }

  return leftover;
}
