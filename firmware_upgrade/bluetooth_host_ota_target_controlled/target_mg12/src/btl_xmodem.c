/***************************************************************************//**
 * @file btl_xmodem.c
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

#include <stdint.h>
#include "ncp_usart.h"
#include "uartdrv.h"
#include "udelay.h"
#include "btl_config.h"

#define SOH     0x01
#define EOT     0x04
#define ACK     0x06
#define NAK     0x15
#define ETB     0x17
#define CAN     0x18
#define C       0x43
#define EOF     0x1A

#define XMODEM_DATA_SIZE  128

// 128 byte data packets
// This is not what the actual packet looks like
/// XMODEM packet
typedef struct {
  uint8_t header;                   ///< Packet header (@ref XMODEM_CMD_SOH)
  uint8_t packetNumber;             ///< Packet sequence number
  uint8_t packetNumberC;            ///< Complement of packet sequence number
  uint8_t data[XMODEM_DATA_SIZE];   ///< Payload
  uint8_t crcH;                     ///< CRC high byte
  uint8_t crcL;                     ///< CRC low byte
} SL_ATTRIBUTE_PACKED XmodemPacket_t;

static XmodemPacket_t packet;

// http://web.mit.edu/6.115/www/amulet/xmodem.htm
int32_t calcrc(int8_t *ptr, int32_t count)
{
  int32_t  crc;
  int8_t i;
  crc = 0;
  while (--count >= 0)
  {
    crc = crc ^ (int32_t) *ptr++ << 8;
    i = 8;
    do
    {
      if (crc & 0x8000)
          crc = crc << 1 ^ 0x1021;
      else
          crc = crc << 1;
    } while(--i);
  }
  return (crc);
}

/* Keep sending until ACK has been received. */
uint32_t send_until_ack(uint8_t *data, uint32_t length)
{
  uint8_t success = 0;
  uint8_t response = 0;

  do{
    /* Clean rx and tx queue. */
    UARTDRV_Abort(handle, uartdrvAbortAll);

    // Send off packet
    UARTDRV_ForceTransmit(handle, data, length);

    /* Clean rx queue. */
    UARTDRV_Abort(handle, uartdrvAbortReceive);
//    UARTDRV_Abort(handle, uartdrvAbortReceive);

    /* Check if ACK received. */
    UARTDRV_ReceiveB(handle, &response, 1);

    switch(response){
      case ACK:
        success = 1;
        break;

      case CAN:
        return 0;

      default:

        break;
    }

  }while(success == 0);

  return 1;
}

uint32_t transmit_SOH_packet(uint8_t *data, uint32_t length, uint8_t number)
{
  packet.header = SOH;
  packet.packetNumber = number;
  packet.packetNumberC = ~number;

  /* Add 0x1A (EOF) padding if the data is less than 128 bytes. */
  memset((packet.data), EOF, XMODEM_DATA_SIZE);
  memcpy((packet.data), data, length);

  packet.crcH = (uint8_t)((calcrc((int8_t *)(packet.data), XMODEM_DATA_SIZE) >> 8) & 0x00FF);
  packet.crcL = (uint8_t)(calcrc((int8_t *)(packet.data), XMODEM_DATA_SIZE) & 0x00FF);
  return send_until_ack((uint8_t *)&packet, XMODEM_DATA_SIZE + 3 + 2);
}

uint32_t bootloader_xmodemSend(uint8_t *data, uint32_t length)
{
  uint32_t totalPackets = length / 128;
  uint32_t success = 0;

  /* 128 bytes of data are sent at a time. */
  uint16_t packetNumber = 1;
  for(int i = 0; i < totalPackets; i++) {
    success = transmit_SOH_packet(data + (i * XMODEM_DATA_SIZE),
                                  XMODEM_DATA_SIZE,
                                  (uint8_t)packetNumber);
    if(success == 0)
      return 0;

    packetNumber = (packetNumber + 1) & 0x00FF;
  }

  /* Last packet may not be 128 bytes long. */
  if(length % 128) {
    success = transmit_SOH_packet(data + (totalPackets * XMODEM_DATA_SIZE),
                                  length % XMODEM_DATA_SIZE,
                                  (uint8_t)((packetNumber) & 0x00FF));

    if(success == 0)
      return 0;
  }

  uint8_t header = EOT;
  return send_until_ack(&header, 1);
}
