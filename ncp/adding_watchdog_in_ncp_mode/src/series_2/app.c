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
#include "app_assert.h"
#include "app.h"

#include "em_cmu.h"
#include "em_wdog.h"
#include "em_rmu.h"

/**************************************************************************//**
 * Define
 *****************************************************************************/
#define TICKS_PER_SECOND    32768
#define TIMER_CALLBACK      1


/**************************************************************************//**
 * Local variable
 *****************************************************************************/
static uint32_t reset_cause;
static const WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;


sl_sleeptimer_timer_handle_t wdog_timer;

void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);
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
  if (reset_cause & EMU_RSTCAUSE_WDOG0) {
      /* watchdog reset occured */
      /* send message to host */
      sl_bt_send_evt_user_message_to_host((uint8_t) sizeof(watchdog_reset_message),
                                          watchdog_reset_message);
  }

  CMU_ClockEnable(cmuClock_WDOG0, true);
  CMU_ClockSelectSet(cmuClock_WDOG0, cmuSelect_ULFRCO); /* ULFRCO as clock source */
  // Init Watchdog driver
  WDOGn_Init(DEFAULT_WDOG, &init);

  // Start the timer to feed the watchdog every 1s
  sc = sl_sleeptimer_start_periodic_timer(&wdog_timer, TICKS_PER_SECOND, sleeptimer_callback, (void*)NULL, 0, 0);
  app_assert_status(sc);
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
  sl_status_t sc;
  uint8array *cmd = (uint8array *)data;
  user_cmd_t *user_cmd = (user_cmd_t *)cmd->data;

  switch (user_cmd->hdr) {
    // -------------------------------
    // Example: user command 1.
    case USER_CMD_1_ID:
      sc = sl_sleeptimer_stop_timer(&wdog_timer);
      sl_ncp_user_cmd_message_to_target_rsp(sc, 0, NULL);
      break;

    default:
      // Send error response to NCP host.
      sl_ncp_user_cmd_message_to_target_rsp(SL_STATUS_FAIL, 0, NULL);
      break;
  }
}

/***************************************************************************//**
 * Sleeptimer callback
 *
 * @param[in] handle Handle of the sleeptimer instance
 * @param[in] data  Callback data
 ******************************************************************************/
void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data){
  (void)handle;
  (void)data;

  sl_bt_external_signal(TIMER_CALLBACK);
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
    case sl_bt_evt_system_external_signal_id:
      // Feed the watchdog
      WDOGn_Feed(DEFAULT_WDOG);
      break;
  }

  return return_value;
}
