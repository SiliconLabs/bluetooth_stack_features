/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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
#include "em_common.h"
#include "sl_ncp.h"
#include "sl_app_assert.h"
#include "app.h"

#include "em_wdog.h"
#include "em_rmu.h"

/**************************************************************************//**
 * Define
 *****************************************************************************/
#define TICKS_PER_SECOND    32768

/**************************************************************************//**
 * Local variable
 *****************************************************************************/
static uint32_t reset_cause;
static const WDOG_Init_TypeDef init =
{
 .enable = true,             /* Start watchdog when init done */
 .debugRun = false,          /* WDOG not counting during debug halt */
 .em2Run = true,             /* WDOG counting when in EM2 */
 .em3Run = true,             /* WDOG counting when in EM3 */
 .em4Block = false,          /* EM4 can be entered */
 .swoscBlock = false,        /* Do not block disabling LFRCO/LFXO in CMU */
 .lock = false,              /* Do not lock WDOG configuration (if locked, reset needed to unlock) */
 .clkSel = wdogClkSelULFRCO, /* Select 1kHZ WDOG oscillator */
 .perSel = wdogPeriod_2k,    /* Set the watchdog period to 2049 clock periods (ie ~2 seconds)*/
};

/***************************************************************************//**
 * Application Init.
 ******************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  sl_status_t sc;
  uint8_t watchdog_reset_message[] = "Watchdog reset";
  // Get the reset cause
  reset_cause = RMU_ResetCauseGet();
  RMU_ResetCauseClear();
  if (reset_cause & RMU_RSTCAUSE_WDOGRST) {
      /* watchdog reset occured */
      /* send message to host */
      sl_bt_send_evt_user_message_to_host((uint8_t) sizeof(watchdog_reset_message),
                                          watchdog_reset_message);
  }

  // init Watchdog driver
  WDOG_Init(&init);

  // Start the software timer to feed the watchdog every 1s
  sc = sl_bt_system_set_soft_timer(TICKS_PER_SECOND,0,0);
  sl_app_assert(sc == SL_STATUS_OK,
                "[E: 0x%04x] Failed to start soft timer\n",
                (int)sc);
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/***************************************************************************//**
 * User command (message_to_target) handler callback.
 *
 * Handles user defined commands received from NCP host.
 * The user commands handled here are defined in app.h and are solely meant for
 * example purposes.
 * @param[in] data Data received from NCP host.
 *
 * @note This overrides the dummy weak implementation.
 ******************************************************************************/
void sl_ncp_user_cmd_message_to_target_cb(void *data)
{
  uint8array *cmd = (uint8array *)data;
  user_cmd_t *user_cmd = (user_cmd_t *)cmd->data;

  switch (user_cmd->hdr) {
    // -------------------------------
    // Example: user command 1.
    case USER_CMD_1_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////

      // Send response to user command 1 to NCP host.
      // Example: sending back received command.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_OK, cmd->len, cmd->data);
      break;

    // -------------------------------
    // Example: user command 2.
    case USER_CMD_2_ID:
      //////////////////////////////////////////////
      // Add your user command handler code here! //
      //////////////////////////////////////////////

      // Send response to user command 2 to NCP host.
      // Example: sending back received command.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_OK, cmd->len, cmd->data);
      // Send user event too.
      // Example: sending back received command as an event.
      sl_ncp_user_evt_message_to_target(cmd->len, cmd->data);
      break;

    // -------------------------------
    // Unknown user command.
    default:
      // Send error response to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_FAIL, 0, NULL);
      break;
  }
}

/**************************************************************************//**
 * Local event processor.
 *
 * Use this function to process Bluetooth stack events locally on NCP.
 * Set the return value to true, if the event should be forwarded to the
 * NCP-host. Otherwise, the event is discarded locally.
 * Implement your own function to override this default weak implementation.
 *
 * @param[in] evt The event.
 *
 * @return true, if we shall send the event to the host. This is the default.
 *
 * @note Weak implementation.
 *****************************************************************************/
bool sl_ncp_local_evt_process(sl_bt_msg_t *evt)
{
  bool return_value = true;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event is actived by the software timer
    case sl_bt_evt_system_soft_timer_id:
      // Feed the watchdog
      WDOG_Feed();
      break;
  }

  return return_value;
}
