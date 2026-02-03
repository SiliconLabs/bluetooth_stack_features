/***************************************************************************//**
 * @file
 * @brief Application interface provided to main().
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

#ifndef APP_H
#define APP_H

#include "sl_bt_api.h"
#include "app_log.h"

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

#define MAX_ADV_DATA_LENGTH 31
#define MAX_EXTENDED_ADV_LENGTH 253 /* Current SDK only support 253 bytes */

/**
 * @brief demo_setup_adv - this function demonstrates
 * 1> set the advertising data with 3 elements - flag, complete local name and more 128 uuids,
 * 2> set the scan response with one element - Manufacturer specific data.
 */
void demo_setup_adv(uint8_t handle);

#endif // APP_H
