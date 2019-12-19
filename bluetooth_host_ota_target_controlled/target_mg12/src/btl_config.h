/*
 * btl_config.h
 *
 *  Created on: Jun 26, 2019
 *      Author: siwoo
 */

#ifndef BTL_APP_BTL_CONFIG_H_
#define BTL_APP_BTL_CONFIG_H_

#include "em_gpio.h"

/* Edit to match your application. */
#define GPIO_ACTIVATION_PORT    gpioPortC
#define GPIO_ACTIVATION_PIN     9
#define GPIO_ACTIVE_STATE       0

#define RESET_PORT              gpioPortC
#define RESET_PIN               10

#define STORAGE_SLOT_ID         0

/* Do not edit. */
#define TIME_10MS               10 * TIME_1MS
#define TIME_5MS                5 * TIME_1MS
#define TIME_1MS                1000

#define BOOTLOADER_MODE         GPIO_ACTIVE_STATE
#define APP_MODE                !GPIO_ACTIVE_STATE

#endif /* BTL_APP_BTL_CONFIG_H_ */
