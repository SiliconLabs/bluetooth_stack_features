/*
 * btl_gpio.h
 *
 *  Created on: Jun 19, 2019
 *      Author: siwoo
 */

#ifndef BTL_GPIO_H_
#define BTL_GPIO_H_

void bootloader_gpioInit(void);

void bootloader_setGPIOActivation(uint8_t state);

void bootloader_setResetPin(uint8_t state);

#endif /* BTL_GPIO_H_ */
