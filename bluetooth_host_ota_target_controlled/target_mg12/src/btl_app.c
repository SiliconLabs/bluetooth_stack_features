/*
 * btl_app.c
 *
 *  Created on: Jun 24, 2019
 *      Author: siwoo
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "ncp_gecko.h"
#include "gatt_db.h"
#include "ncp.h"
#include "ncp_usart.h"
#include "udelay.h"
#include "btl_interface.h"
#include "btl_interface_storage.h"
#include "btl_gpio.h"
#include "btl_app.h"
#include "btl_xmodem.h"
#include "btl_config.h"
#include "uartdrv.h"

static uint8_t ota_connection = 0;
static uint32_t ota_mode = 0;
static uint32_t ota_data_len = 0;

void handle_bootloader_cmds(void);

void bootloader_appInit(void)
{
  bootloader_init();
  bootloader_gpioInit();
  UDELAY_Calibrate();

  /* Make sure GPIO activation pin is low so NCP host won't go into BootLoader mode. */
  bootloader_setGPIOActivation(APP_MODE);
  // Delay to stabilize
  UDELAY_Delay(TIME_5MS);

  /* Make sure reset pin is high so NCP host can run. */
  bootloader_setResetPin(1);
  // Delay to stabilize
  UDELAY_Delay(TIME_5MS);
}

uint32_t bootloader_handle_event(struct gecko_cmd_packet *evt)
{
  bool evt_handled = false;
  switch(BGLIB_MSG_ID(evt->header)) {
    case gecko_evt_system_boot_id:
      gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);
      gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
      break;

    case gecko_evt_le_connection_opened_id:
      if(ota_mode != 0) {
        gecko_cmd_le_connection_set_parameters(ota_connection,6,6,0,1000);
        evt_handled = true;
      }
      break;

    case gecko_evt_gatt_server_user_write_request_id: {
      uint32_t characteristic = evt->data.evt_gatt_server_user_write_request.characteristic;
      uint8_t *data = &(evt->data.evt_gatt_server_user_write_request.value.data[0]);
      uint8_t length = evt->data.evt_gatt_server_user_write_request.value.len;

      if(characteristic == gattdb_ota_control) {
        evt_handled = true;

        /* If control data = 0, then start transfer
         * If control data = 3, then end transfer. */
        switch(data[0]) {
          /* Start Transfer. */
          case 0:
            /* To work with blue gecko app, connection reset must occur. */
            if(ota_mode == 0) {
              ota_connection = evt->data.evt_gatt_server_user_write_request.connection;
              ota_mode = 1;
              ota_data_len = 0;

              /* Erase current contents in storage slot. This may cause errors if not erased. */
              bootloader_eraseStorageSlot(STORAGE_SLOT_ID);

              /* Send success response. */
              gecko_cmd_gatt_server_send_user_write_response(ota_connection,
                                                             characteristic,
                                                             bg_err_success);

              /* Close connection. */
              gecko_cmd_le_connection_close(ota_connection);
              break;
            }

            /* This line is reached when a connection reset has occurred. */
            ota_data_len = 0;
            gecko_cmd_gatt_server_send_user_write_response(ota_connection,
                                                           characteristic,
                                                           bg_err_success);
            break;

          /* End Transfer. */
          case 3:
            ota_mode = 0;
            gecko_cmd_gatt_server_send_user_write_response(ota_connection,
                                                           characteristic,
                                                           bg_err_success);

            /* Verify Image */
            int32_t ret = bootloader_verifyImage(STORAGE_SLOT_ID, 0);

            if(ret != BOOTLOADER_OK){
              gecko_cmd_le_gap_start_advertising(0, le_gap_discover_generic, le_gap_connectable_scannable);
              break;
            }

            bootloader_appTransferImage();
            gecko_cmd_le_gap_start_advertising(0, le_gap_discover_generic, le_gap_connectable_scannable);
            break;
        }
      }

      // Store image into a storage slot
      else if(characteristic == gattdb_ota_data) {
        evt_handled = true;
        bootloader_writeStorage(STORAGE_SLOT_ID,
                                ota_data_len,
                                data,
                                length);
        gecko_cmd_gatt_server_send_user_write_response(ota_connection,
                                                       characteristic,
                                                       bg_err_success);
        ota_data_len += length;
      }

      break;
    }

    case gecko_evt_le_connection_closed_id:
      if(ota_mode != 0){
        gecko_cmd_le_gap_start_advertising(0, le_gap_discover_generic, le_gap_connectable_scannable);
        evt_handled = true;
      }
      break;

    default:
      break;
  }

  return evt_handled;
}

void bootloader_appTransferImage(void)
{
  /* Prevent BootLoader from getting any weird data. */
  tx_queue_reset();
  UARTDRV_Abort(handle, uartdrvAbortAll);

  bootloader_setGPIOActivation(BOOTLOADER_MODE);
  UDELAY_Delay(TIME_5MS);

  /* Press and release reset pin. */
  bootloader_setResetPin(0);
  UDELAY_Delay(TIME_5MS);
  bootloader_setResetPin(1);

  /* The BootLoader will print some user commands. */
  handle_bootloader_cmds();
}

/* Wait until device is ready to receive a command.
 * Look in AN0003 for what is printed by BootLoader. */
void wait_for_char(uint8_t character)
{
  uint8_t response = 0;

  do{
    /* Clean rx queue. */
    UARTDRV_Abort(handle, uartdrvAbortAll);

    UARTDRV_ReceiveB(handle, &response, 1);

  } while(response != character);
}

void handle_bootloader_cmds(void)
{
  uint8_t command = 0;

  /* Wait until menu is printed. */
  wait_for_char(0);

  /* Send upload gbl command.
   * Needs to be char 1 due to user keyboard inputs are only accepted. */
  command = '1';
  UARTDRV_TransmitB(handle, &command, 1);

  /* Wait until response is received. */
  wait_for_char(0);

  /* Wait until C command has been received from host. Signifies CRC use. */
  wait_for_char('C');

  /* Get image address. */
  BootloaderStorageSlot_t slotInfo;
  bootloader_getStorageSlotInfo(STORAGE_SLOT_ID, &slotInfo);

  /* Transmit gbl file, make it blocking so no USART transfers occur. */
  if(bootloader_xmodemSend((uint8_t *)slotInfo.address, ota_data_len) == 0){
    /* Failed. */
    /* Set gpio activation pin back to low. */
    bootloader_setGPIOActivation(APP_MODE);
    UDELAY_Delay(TIME_5MS);

    /* Press and release reset pin. */
    bootloader_setResetPin(0);
    UDELAY_Delay(TIME_5MS);
    bootloader_setResetPin(1);
    return;
  }

  /* Wait until ready. */
  wait_for_char(0);

  /* Wait until menu is printed. */
  wait_for_char(0);

  /* Set gpio activation pin back to low. */
  bootloader_setGPIOActivation(APP_MODE);
  UDELAY_Delay(TIME_5MS);

  /* Send run application command. */
  command = '2';
  UARTDRV_TransmitB(handle, &command, 1);
}
