/*
 * btl_gpio.c
 *
 *  Created on: Jun 19, 2019
 *      Author: siwoo
 */

#include <stdint.h>
#include "em_cmu.h"
#include "em_gpio.h"
#include "btl_config.h"

void bootloader_gpioInit(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  GPIO_PinModeSet(GPIO_ACTIVATION_PORT,
                  GPIO_ACTIVATION_PIN,
                  gpioModePushPull,
                  !GPIO_ACTIVE_STATE);

  /* Make sure there the reset pin has a pull up. This will prevent unintentional
   * resets to occur during initialization. */
  GPIO_PinModeSet(RESET_PORT, RESET_PIN, gpioModePushPull, 1);
}

void bootloader_setGPIOActivation(uint8_t state)
{
  if(state != 0)
    GPIO_PinOutSet(GPIO_ACTIVATION_PORT, GPIO_ACTIVATION_PIN);
  else
    GPIO_PinOutClear(GPIO_ACTIVATION_PORT, GPIO_ACTIVATION_PIN);
}

void bootloader_setResetPin(uint8_t state)
{
  if(state != 0)
    GPIO_PinOutSet(RESET_PORT, RESET_PIN);
  else
    GPIO_PinOutClear(RESET_PORT, RESET_PIN);
}
