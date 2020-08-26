/***************************************************************************//**
 * @file btl_config.h
 * @version 1.0.1
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
 *******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

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
