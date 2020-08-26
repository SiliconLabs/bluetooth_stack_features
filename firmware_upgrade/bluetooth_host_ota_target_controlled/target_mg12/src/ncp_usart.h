/***************************************************************************//**
 * @file ncp_usart.h
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

#ifndef NCP_UART_H_
#define NCP_UART_H_

#include "hal-config.h"
#include "ncp.h"
#include "uartdrv.h"

#if BSP_UARTNCP_USART_PORT == HAL_SERIAL_PORT_USART0
// USART0
#define NCP_USART_UART        USART0
#define NCP_USART_CLK         cmuClock_USART0
#define NCP_USART_IRQ_NAME    USART0_RX_IRQHandler
#define NCP_USART_IRQn        USART0_RX_IRQn
#define NCP_USART_TX_IRQ_NAME   USART0_TX_IRQHandler
#define NCP_USART_TX_IRQn       USART0_TX_IRQn
#define NCP_USART_USART       0
#elif BSP_UARTNCP_USART_PORT == HAL_SERIAL_PORT_USART1
// USART1
#define NCP_USART_UART        USART1
#define NCP_USART_CLK         cmuClock_USART1
#define NCP_USART_IRQ_NAME    USART1_RX_IRQHandler
#define NCP_USART_IRQn        USART1_RX_IRQn
#define NCP_USART_TX_IRQ_NAME   USART1_TX_IRQHandler
#define NCP_USART_TX_IRQn       USART1_TX_IRQn
#define NCP_USART_USART      1
#elif BSP_UARTNCP_USART_PORT == HAL_SERIAL_PORT_USART2
// USART2
#define NCP_USART_UART        USART2
#define NCP_USART_CLK         cmuClock_USART2
#define NCP_USART_IRQ_NAME    USART2_RX_IRQHandler
#define NCP_USART_IRQn        USART2_RX_IRQn
#define NCP_USART_TX_IRQ_NAME   USART2_TX_IRQHandler
#define NCP_USART_TX_IRQn       USART2_TX_IRQn
#define NCP_USART_USART       2
#elif BSP_UARTNCP_USART_PORT == HAL_SERIAL_PORT_USART3
// USART3
#define NCP_USART_UART        USART3
#define NCP_USART_CLK         cmuClock_USART3
#define NCP_USART_IRQ_NAME    USART3_RX_IRQHandler
#define NCP_USART_IRQn        USART3_RX_IRQn
#define NCP_USART_TX_IRQ_NAME   USART3_TX_IRQHandler
#define NCP_USART_TX_IRQn       USART3_TX_IRQn
#define NCP_USART_USART       3
#else
#error "Unsupported UART port!"
#endif

#define NCP_USART_WAKEUP_SIGNAL         (1 << 0)
#define NCP_USART_UPDATE_SIGNAL         (1 << 1)
#define NCP_USART_TIMEOUT_SIGNAL        (1 << 2)
#define NCP_USART_SECURITY_SIGNAL       (1 << 3)

extern UARTDRV_Handle_t handle;

/***************************************************************************//**
 * @brief
 *   Initialize USART for NCP.
 *
 * @details
 *   This function will initialize USART peripheral and start receiving NCP commands.
 *
 ******************************************************************************/
void ncp_usart_init();

/***************************************************************************//**
 * @brief
 *   Update USART status for sleep/wakeup.
 *
 * @details
 *   This function will handle USART_UPDATE signal and update USART status accordingly.
 *
 ******************************************************************************/
void ncp_usart_status_update();

/***************************************************************************//**
 * @brief
 *   This function should be called in application's event loop.
 *
 * @details
 *   This function will handle an event and return if the event was processed by
 *    the library or not.
 *
 * @param[in] evt
 *   Pointer to the event received from the Bluetooth stack.
 *
 * @return
 *   True if the event was processed, False otherwise.
 *
 ******************************************************************************/
bool ncp_handle_event(struct gecko_cmd_packet *evt);

#endif /* NCP_UART_H_ */
