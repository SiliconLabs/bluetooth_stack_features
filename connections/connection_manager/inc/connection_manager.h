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

/***************************************************************************//**
 * @brief Data structure of the connection pool
 ******************************************************************************/
typedef struct {
  bd_addr address;
  uint8_t address_type;
  uint8_t master;
  uint8_t bonding;
  uint8_t advertiser;
  uint16_t interval;
  uint16_t latency;
  uint16_t timeout;
  uint16_t txsize;
  uint8_t  security_mode;
  uint8_t handle;
} connection_t;

void sli_bt_cm_init(void);
void sli_bt_cm_on_event(sl_bt_msg_t *evt);

sl_status_t sli_bt_cm_add_connection(sl_bt_evt_connection_opened_t *evt_data);
sl_status_t sli_bt_cm_update_parameters(sl_bt_evt_connection_parameters_t *evt_data);
sl_status_t sli_bt_cm_update_bonding(sl_bt_evt_sm_bonded_t *evt_data);
sl_status_t sli_bt_cm_remove_connection(sl_bt_evt_connection_closed_t *evt_data);

/***************************************************************************//**
 *
 * Retrieve all the connection handles from the connection pool
 *
 * In the first argument, provide the initial array to place the connection
 * handles into. In the second, place the size of your initial array, after
 * the function call this will change to the number of the valid handles
 * in the array.
 *
 * SL_STATUS_WOULD_OVERFLOW will be returned, if the number of handles would
 * exceed the size of the provided array, SL_STATUS_EMPTY if no valid handles
 * to be found.
 *
 * @param[out] connection_handles Array to store the handles
 * @param[in/out] size Length of data in @p connection_handles
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 *
 ******************************************************************************/
sl_status_t sl_bt_cm_get_connection_handles(uint8_t *connection_handles, uint8_t *size);

/***************************************************************************//**
 *
 * Retrieve a connection by the provided handle
 *
 * In the first argument, provide the handle for the connection you are looking
 * for, in the second one, provide a pointer for the connection itself.
 *
 * SL_STATUS_NOT_FOUND will be returned, if no connection to be found.
 *
 * @param[in] connection_handle Handle to look for
 * @param[out] connection Pointer to the connection
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 *
 ******************************************************************************/
sl_status_t sl_bt_cm_get_connection_by_handle(uint8_t connection_handle, connection_t **connection);

/***************************************************************************//**
 *
 * Retrieve a connection by the provided address
 *
 * In the first argument, provide the address pointer for the connection you are
 * looking for, in the second one, provide a pointer for the connection itself.
 *
 * SL_STATUS_NOT_FOUND will be returned, if no connection to be found.
 *
 * @param[in] address Pointer to the address to look for
 * @param[out] connection Pointer to the connection
 *
 * @return SL_STATUS_OK if successful. Error code otherwise.
 *
 ******************************************************************************/
sl_status_t sl_bt_cm_get_connection_by_address(bd_addr *address, connection_t **connection);

/***************************************************************************//**
 *
 * Retrieve the connection pool state
 *
 * @return true if the pool is full, false otherwise.
 *
 ******************************************************************************/
bool sl_bt_cm_is_connection_list_full(void);

/***************************************************************************//**
 *
 * Retrieve the leftover space in the connection pool
 *
 * @return The amount of places left in the pool
 *
 ******************************************************************************/
uint8_t sl_bt_cm_get_leftover_space(void);
