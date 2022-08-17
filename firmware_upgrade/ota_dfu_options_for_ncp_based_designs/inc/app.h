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

// Example: user command 1.
#define USER_CMD_1_ID    0x01
typedef uint8_t cmd_1_t[16];

// Example: user command 2.
#define USER_CMD_2_ID    0x02
typedef uint8_t cmd_2_t[8];

PACKSTRUCT(struct user_cmd {
  uint8_t hdr;
  // Example: union of user commands.
  union {
    cmd_1_t cmd_1;
    cmd_2_t cmd_2;
  } data;
});

typedef struct user_cmd user_cmd_t;

/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
void app_init(void);

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
void app_process_action(void);

#endif // APP_H
