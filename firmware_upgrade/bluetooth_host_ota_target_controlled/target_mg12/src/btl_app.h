/*
 * btl_app.h
 *
 *  Created on: Jun 24, 2019
 *      Author: siwoo
 */

#ifndef BTL_APP_BTL_APP_H_
#define BTL_APP_BTL_APP_H_

void bootloader_appInit(void);

uint32_t bootloader_handle_event(struct gecko_cmd_packet *evt);

void bootloader_appTransferImage(void);

uint8_t bootloader_isTransferReady(void);

#endif /* BTL_APP_BTL_APP_H_ */
