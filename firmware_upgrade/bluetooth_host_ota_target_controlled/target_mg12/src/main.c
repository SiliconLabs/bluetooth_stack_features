/***************************************************************************//**
 * @file main.c
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

#include "init_mcu.h"
#include "init_board.h"
#include "init_app.h"
#include "ble-configuration.h"
#include "board_features.h"

/* BG stack headers */
#include "bg_types.h"
#include "ncp_gecko.h"
#include "gatt_db.h"
#include "ncp_usart.h"
#include "em_core.h"

/* libraries containing default gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"

/* Device initialization header */
#include "hal-config.h"

#ifdef FEATURE_BOARD_DETECTED
#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif // HAL_CONFIG
#endif // FEATURE_BOARD_DETECTED

#include "btl_app.h"

/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 8
#endif
#ifndef MAX_ADVERTISERS
#define MAX_ADVERTISERS 2
#endif

#if defined(NCP_DEEP_SLEEP_ENABLED)
#define NCP_TX_WATCH_COUNTER_MAX    1000
#else
#define NCP_TX_WATCH_COUNTER_MAX    0xffff
#endif

uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];
static int ncp_tx_queue_watch_counter = 0;

// Gecko configuration parameters (see gecko_configuration.h)
static const gecko_configuration_t config = {
  .config_flags = 0,
#if defined(FEATURE_LFXO) && defined(NCP_DEEP_SLEEP_ENABLED)
  .sleep.flags = SLEEP_FLAGS_DEEP_SLEEP_ENABLE,
#else
  .sleep.flags = 0,
#endif
  .bluetooth.max_connections = MAX_CONNECTIONS,
  .bluetooth.max_advertisers = MAX_ADVERTISERS,
  .bluetooth.heap = bluetooth_stack_heap,
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap),
  .bluetooth.sleep_clock_accuracy = 100, // ppm
  .gattdb = &bg_gattdb_data,
#if (HAL_PA_ENABLE)
  .pa.config_enable = 1, // Set this to be a valid PA config
#if defined(FEATURE_PA_INPUT_FROM_VBAT)
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT, // Configure PA input to VBAT
#else
  .pa.input = GECKO_RADIO_PA_INPUT_DCDC,
#endif // defined(FEATURE_PA_INPUT_FROM_VBAT)
#endif // (HAL_PA_ENABLE)
  .rf.flags = GECKO_RF_CONFIG_ANTENNA,                 /* Enable antenna configuration. */
  .rf.antenna = GECKO_RF_ANTENNA,                      /* Select antenna path! */
};

/**
 * Handle events meant to be handled locally
 */
static uint32_t local_handle_event(struct gecko_cmd_packet *evt)
{
  bool evt_handled = bootloader_handle_event(evt);

  switch (BGLIB_MSG_ID(evt->header)) {
    default:
      break;
  }
  return evt_handled;
}

/*
 * Check if the TX queue has enough space for a new event message.
 * A counter is used here for catching a rare situation where the NCP gets stuck somehow.
 */
static bool check_ncp_tx_queue_space()
{
  if (ncp_get_transmit_space_for_events() < NCP_CMD_SIZE) {
    ++ncp_tx_queue_watch_counter;
    if (ncp_tx_queue_watch_counter > NCP_TX_WATCH_COUNTER_MAX) {
      // Fatal error, or the NCP transport has been shut down for too long.
      // Reset the queue for keeping the device responsive in case the stack is still running in background.
      tx_queue_reset();
      ncp_tx_queue_watch_counter = 0;
      return true;
    }
    return false;
  } else {
    ncp_tx_queue_watch_counter = 0;
    return true;
  }
}

/**
 * Extract the next BGAPI event only when there is enough space for buffering it in TX queue.
 */
static struct gecko_cmd_packet *get_bgapi_event()
{
  if (check_ncp_tx_queue_space()) {
    return gecko_peek_event();
  }
  return NULL;
}

int main(void)
{
  // Initialize device
  initMcu();
  // Initialize board
  initBoard();
  // NCP USART init
  ncp_usart_init();
  // Initialize application
  initApp();

  bootloader_appInit();

  // Initialize stack
  gecko_init(&config);

  while (1) {
    struct gecko_cmd_packet *evt;
    ncp_handle_command();
    for (evt = get_bgapi_event(); evt != NULL; evt = get_bgapi_event()) {
      if (!ncp_handle_event(evt) && !local_handle_event(evt)) {
        // send out the event if not handled either by NCP or locally
        ncp_transmit_enqueue(evt);
      }
      // if a command is received, break and handle the command
      if (ncp_command_received()) {
        break;
      }
    }
    ncp_transmit();

    // If an NCP command received, do not sleep
    if (!ncp_command_received()) {
      CORE_CRITICAL_IRQ_DISABLE();
      gecko_sleep_for_ms(gecko_can_sleep_ms());
      CORE_CRITICAL_IRQ_ENABLE();
    }
  }
}

/** @} (end addtogroup app) */
/** @} (end addtogroup Application) */
