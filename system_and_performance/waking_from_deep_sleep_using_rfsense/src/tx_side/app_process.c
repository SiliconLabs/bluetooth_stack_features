/***************************************************************************//**
 * @file app_tick.c
 * @brief app_tick.c
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

// -----------------------------------------------------------------------------
//                                   Includes
// -----------------------------------------------------------------------------
#include <stdint.h>
#include "sl_component_catalog.h"
#include "app_assert.h"
#include "app_log.h"
#include "rail.h"
#include "app_process.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "rail_config.h"
#include "sl_flex_packet_asm.h"

// -----------------------------------------------------------------------------
//                              Macros and Typedefs
// -----------------------------------------------------------------------------
/// Size of RAIL RX/TX FIFO
#define RAIL_FIFO_SIZE (256u)
/// Transmit data length
#define TX_PAYLOAD_LENGTH (16u)

/// State machine of simple_trx
typedef enum {
  S_PACKET_RECEIVED,
  S_PACKET_SENT,
  S_RX_PACKET_ERROR,
  S_TX_PACKET_ERROR,
  S_CALIBRATION_ERROR,
  S_IDLE,
} state_t;

// -----------------------------------------------------------------------------
//                          Static Function Declarations
// -----------------------------------------------------------------------------
/**************************************************************************//**
 * The function printfs the received rx message.
 *
 * @param rx_buffer Msg buffer
 * @param length How many bytes should be printed out
 * @returns None
 *****************************************************************************/
static void printf_rx_packet(const uint8_t * const rx_buffer, uint16_t length);

/******************************************************************************
 * The API helps to unpack the received packet, point to the payload and returns the length.
 *
 * @param rx_destination Where should the full packet be unpacked
 * @param packet_information Where should all the information of the packet stored
 * @param start_of_payload Pointer where the payload starts
 * @return The length of the received payload
 *****************************************************************************/
static uint16_t unpack_packet(uint8_t *rx_destination, const RAIL_RxPacketInfo_t *packet_information, uint8_t **start_of_payload);

/******************************************************************************
 * The API prepares the packet for sending and load it in the RAIL TX FIFO
 *
 * @param rail_handle Which rail handlers should be used for the TX FIFO writing
 * @param out_data The payload buffer
 * @param length The length of the payload
 *****************************************************************************/
//static void prepare_package(RAIL_Handle_t rail_handle, uint8_t *out_data, uint16_t length);

// -----------------------------------------------------------------------------
//                                Global Variables
// -----------------------------------------------------------------------------
/// Flag, indicating transmit request (button has pressed / CLI transmit request has occured)
volatile bool tx_requested = false;
/// Flag, indicating received packet is forwarded on CLI or not
volatile bool rx_requested = false;

// -----------------------------------------------------------------------------
//                                Static Variables
// -----------------------------------------------------------------------------
/// The variable shows the actual state of the state machine
static volatile state_t state = S_IDLE;

/// Contains the last RAIL Rx/Tx error events
static volatile uint64_t error_code = 0;

/// Contains the status of RAIL Calibration
static volatile RAIL_Status_t calibration_status = 0;

/// RAIL Rx packet handle
static volatile RAIL_RxPacketHandle_t rx_packet_handle;

/// Receive and Send FIFO
static uint8_t rx_fifo[RAIL_FIFO_SIZE];
static uint8_t tx_fifo[RAIL_FIFO_SIZE];

#define NUMSYNCWORDBYTES (2U) // Syncword Length in bytes, 1-4 bytes.
#define SYNCWORD         (0xB16FU) // Syncword Value
/// Transmit packet
//static uint8_t out_packet[NUMSYNCWORDBYTES] = {0xB1, 0x6F};

/// Flags to update state machine from interrupt
static volatile bool packet_recieved = false;
static volatile bool packet_sent = false;
static volatile bool rx_error = false;
static volatile bool tx_error = false;
static volatile bool cal_error = false;

// -----------------------------------------------------------------------------
//                          Public Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * Application state machine, called infinitely
 *****************************************************************************/
void app_process_action(RAIL_Handle_t rail_handle)
{
  RAIL_RxPacketInfo_t packet_info;
  // Status indicator of the RAIL API calls
  RAIL_Status_t rail_status = RAIL_STATUS_NO_ERROR;
  RAIL_Status_t calibration_status_buff = RAIL_STATUS_NO_ERROR;

  if (packet_recieved) {
    packet_recieved = false;
    state = S_PACKET_RECEIVED;
  } else if (packet_sent) {
    packet_sent = false;
    state = S_PACKET_SENT;
  } else if (rx_error) {
    rx_error = false;
    state = S_RX_PACKET_ERROR;
  } else if (tx_error) {
    tx_error = false;
    state = S_TX_PACKET_ERROR;
  } else if (cal_error) {
    cal_error = false;
    state = S_CALIBRATION_ERROR;
  }

  switch (state) {
    case S_PACKET_RECEIVED:
      // Packet received:
      //  - Check whether RAIL_HoldRxPacket() was successful, i.e. packet handle is valid
      //  - Copy it to the application FIFO
      //  - Free up the radio FIFO
      //  - Return to IDLE state i.e. RAIL Rx
      rx_packet_handle = RAIL_GetRxPacketInfo(rail_handle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE, &packet_info);
      while (rx_packet_handle != RAIL_RX_PACKET_HANDLE_INVALID) {
        uint8_t *start_of_packet = 0;
        uint16_t packet_size = unpack_packet(rx_fifo, &packet_info, &start_of_packet);
        rail_status = RAIL_ReleaseRxPacket(rail_handle, rx_packet_handle);

        app_assert(rail_status == RAIL_STATUS_NO_ERROR, "RAIL_ReleaseRxPacket() result:%d", rail_status);

        if (rx_requested) {
          printf_rx_packet(start_of_packet, packet_size);
        }
        sl_led_toggle(&sl_led_led0);
        rx_packet_handle = RAIL_GetRxPacketInfo(rail_handle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE, &packet_info);
      }
      state = S_IDLE;
      break;
    case S_PACKET_SENT:

      app_log_info("Packet has been sent\r\n");
#if defined(SL_CATALOG_LED1_PRESENT)
      sl_led_toggle(&sl_led_led1);
#else
      sl_led_toggle(&sl_led_led0);
#endif
      state = S_IDLE;
      break;
    case S_RX_PACKET_ERROR:
      // Handle Rx error
      app_log_info("Radio RX Error occurred\nEvents: %llX\n", error_code);
      state = S_IDLE;
      break;
    case S_TX_PACKET_ERROR:
      // Handle Tx error
      app_log_info("Radio TX Error occurred\nEvents: %llX\r\n", error_code);
      state = S_IDLE;
      break;
    case S_IDLE:
      if (tx_requested) {
        RAIL_Idle(rail_handle, RAIL_IDLE_ABORT, true);
        RAIL_ConfigRfSenseSelectiveOokWakeupPhy(rail_handle);
        RAIL_SetRfSenseSelectiveOokWakeupPayload(rail_handle, NUMSYNCWORDBYTES, SYNCWORD);
        rail_status = RAIL_StartTx(rail_handle, CHANNEL, RAIL_TX_OPTIONS_DEFAULT, NULL);

        app_assert(rail_status == RAIL_STATUS_NO_ERROR, "RAIL_StartTx() result:%d ", rail_status);
        tx_requested = false;
      }
      break;
    case S_CALIBRATION_ERROR:
      calibration_status_buff = calibration_status;
      app_assert(true,
                  "Radio Calibration Error occurred\nEvents: %llX\nRAIL_Calibrate() result:%d\r\n",
                  error_code,
                  calibration_status_buff);
      state = S_IDLE;
      break;
    default:
      // Unexpected state
      app_log_info("Unexpected Simple TRX state occured:%d\r\n", state);
      break;
  }
}

/******************************************************************************
 * RAIL callback, called if a RAIL event occurs.
 *****************************************************************************/
void sl_rail_util_on_event(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
  error_code = events;
  // Handle Rx events
  if ( events & RAIL_EVENTS_RX_COMPLETION ) {
    if (events & RAIL_EVENT_RX_PACKET_RECEIVED) {
      // Keep the packet in the radio buffer, download it later at the state machine
      RAIL_HoldRxPacket(rail_handle);
      packet_recieved = true;
    } else {
      // Handle Rx error
      rx_error = true;
    }
  }
  // Handle Tx events
  if ( events & RAIL_EVENTS_TX_COMPLETION) {
    if (events & RAIL_EVENT_TX_PACKET_SENT) {
      packet_sent = true;
    } else {
      // Handle Tx error
      tx_error = true;
    }
  }

  // Perform all calibrations when needed
  if ( events & RAIL_EVENT_CAL_NEEDED ) {
    calibration_status = RAIL_Calibrate(rail_handle, NULL, RAIL_CAL_ALL_PENDING);
    if (calibration_status != RAIL_STATUS_NO_ERROR) {
      cal_error = true;
    }
  }
}

/******************************************************************************
 * Button callback, called if any button is pressed or released.
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    tx_requested = true;
  }
}

/******************************************************************************
 * Set up the rail TX fifo for later usage
 * @param rail_handle Which rail handler should be updated
 *****************************************************************************/
void set_up_tx_fifo(RAIL_Handle_t rail_handle)
{
  uint16_t allocated_tx_fifo_size = 0;
  allocated_tx_fifo_size = RAIL_SetTxFifo(rail_handle, tx_fifo, 0, RAIL_FIFO_SIZE);
  app_assert(allocated_tx_fifo_size == RAIL_FIFO_SIZE,
               "RAIL_SetTxFifo() failed to allocate a large enough fifo (%d bytes instead of %d bytes)\r\n",
               allocated_tx_fifo_size,
               RAIL_FIFO_SIZE);
}

// -----------------------------------------------------------------------------
//                          Static Function Definitions
// -----------------------------------------------------------------------------
/******************************************************************************
 * The API forwards the received rx packet on CLI
 *****************************************************************************/
static void printf_rx_packet(const uint8_t * const rx_buffer, uint16_t length)
{
  uint8_t i = 0;
  app_log_info("Packet has been received: ");
  for (i = 0; i < length; i++) {
   app_log_info("0x%02X, ", rx_buffer[i]);
  }
  app_log_info("\r\n");
}

#if !defined(RAIL0_CHANNEL_GROUP_1_PROFILE_WISUN)
/******************************************************************************
 * The API helps to unpack the received packet, point to the payload and returns the length.
 *****************************************************************************/
static uint16_t unpack_packet(uint8_t *rx_destination, const RAIL_RxPacketInfo_t *packet_information, uint8_t **start_of_payload)
{
  RAIL_CopyRxPacket(rx_destination, packet_information);
  *start_of_payload = rx_destination;
  return ((packet_information->packetBytes > RAIL_FIFO_SIZE) ? RAIL_FIFO_SIZE : packet_information->packetBytes);
}

/******************************************************************************
 * The API prepares the packet for sending and load it in the RAIL TX FIFO
 *****************************************************************************/
/*static void prepare_package(RAIL_Handle_t rail_handle, uint8_t *out_data, uint16_t length)
{
}
*/
#else
/******************************************************************************
 * The API helps to unpack the received packet, point to the payload and returns the length.
 *****************************************************************************/
static uint16_t unpack_packet(uint8_t *rx_destination, const RAIL_RxPacketInfo_t *packet_information, uint8_t **start_of_payload)
{
  sl_flex_802154_packet_mhr_frame_t rx_mhr = { 0 };
  uint16_t payload_size = 0;
  uint8_t rx_phr_config = 0u;
  RAIL_CopyRxPacket(rx_destination, packet_information);

  *start_of_payload = sl_flex_802154_packet_unpack_g_opt_data_frame(&rx_phr_config,
                                                                    &rx_mhr,
                                                                    &payload_size,
                                                                    rx_destination);
  return ((payload_size > (RAIL_FIFO_SIZE - SL_FLEX_IEEE802154_MHR_LENGTH)) ? (RAIL_FIFO_SIZE - SL_FLEX_IEEE802154_MHR_LENGTH) : payload_size);
}

/******************************************************************************
 * The API prepares the packet for sending and load it in the RAIL TX FIFO
 *****************************************************************************/
static void prepaire_package(RAIL_Handle_t rail_handle, uint8_t *out_data, uint16_t length)
{
  // Check if write fifo has written all bytes
  uint16_t bytes_writen_in_fifo = 0;
  uint16_t packet_size = 0u;
  uint8_t tx_phr_config = SL_FLEX_IEEE802154G_PHR_MODE_SWITCH_OFF
                          | SL_FLEX_IEEE802154G_PHR_CRC_4_BYTE
                          | SL_FLEX_IEEE802154G_PHR_DATA_WHITENING_ON;
  sl_flex_802154_packet_mhr_frame_t tx_mhr = {
    .frame_control          = MAC_FRAME_TYPE_DATA                \
                              | MAC_FRAME_FLAG_PANID_COMPRESSION \
                              | MAC_FRAME_DESTINATION_MODE_SHORT \
                              | MAC_FRAME_VERSION_2006           \
                              | MAC_FRAME_SOURCE_MODE_SHORT,
    .sequence_number        = 0u,
    .destination_pan_id     = (0xFFFF),
    .destination_address    = (0xFFFF),
    .source_address         = (0x0000)
  };
  uint8_t tx_frame_buffer[256];
  sl_flex_802154_packet_pack_g_opt_data_frame(tx_phr_config,
                                              &tx_mhr,
                                              length,
                                              out_data,
                                              &packet_size,
                                              tx_frame_buffer);
  bytes_writen_in_fifo = RAIL_WriteTxFifo(rail_handle, tx_frame_buffer, packet_size, true);
  APP_ASSERT(bytes_writen_in_fifo == packet_size,
             "RAIL_WriteTxFifo() failed to write in fifo (%d bytes instead of %d bytes)\n",
             bytes_writen_in_fifo,
             packet_size);
}
#endif
