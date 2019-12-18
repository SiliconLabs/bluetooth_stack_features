/***************************************************************************//**
 * @file
 * @brief Silabs Network Co-Processor (NCP) library USART driver
 * This library allows customers create applications work in NCP mode.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
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
