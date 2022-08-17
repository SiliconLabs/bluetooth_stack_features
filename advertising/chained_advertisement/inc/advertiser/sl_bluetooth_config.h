#ifndef SL_BLUETOOTH_CONFIG_H
#define SL_BLUETOOTH_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> Bluetooth Stack Configuration

// <o SL_BT_CONFIG_MAX_CONNECTIONS> Max number of connections <0-32>
// <i> Default: 4
// <i> Define the number of connections the application needs.
#define SL_BT_CONFIG_MAX_CONNECTIONS     (4)

// <o SL_BT_CONFIG_USER_ADVERTISERS> Max number of advertisers reserved for user <0-8>
// <i> Default: 1
// <i> Define the number of advertisers the application needs.
#define SL_BT_CONFIG_USER_ADVERTISERS     1

#define SL_BT_CONFIG_MAX_ADVERTISERS     (SL_BT_CONFIG_USER_ADVERTISERS + SL_BT_COMPONENT_ADVERTISERS)

// <o SL_BT_CONFIG_MAX_SOFTWARE_TIMERS> Max number of software timers <0-16>
// <i> Default: 4
// <i> Define the number of software timers the application needs.  Each timer needs resources from the stack to be implemented. Increasing amount of soft timers may cause degraded performance in some use cases.
#define SL_BT_CONFIG_MAX_SOFTWARE_TIMERS     (4)

// <o SL_BT_CONFIG_MAX_PERIODIC_ADVERTISING_SYNC> Max number of periodic advertising synchronizations <0-8>
// <i> Default: 0
// <i> The maximum number of periodic advertising synchronizations the Bluetooth stack needs to support.
#define SL_BT_CONFIG_MAX_PERIODIC_ADVERTISING_SYNC     1

// <o SL_BT_CONFIG_BUFFER_SIZE> Buffer memory size for Bluetooth stack
// <i> Default: 3150
// <i> Define buffer memory size for running Bluetooth stack and buffering data over Bluetooth connections,
// <i> advertising and scanning. The default value is an estimation for achieving adequate throughput
// <i> and supporting multiple simultaneous connections. Consider increasing this value for
// <i> higher data throughput over connections, advertising or scanning long advertisement data.
#define SL_BT_CONFIG_BUFFER_SIZE    4000

// </h> End Bluetooth Stack Configuration

// <h> TX Power Levels

// <o SL_BT_CONFIG_MIN_TX_POWER> Minimum radiated TX power level in 0.1dBm unit
// <i> Default: -30 (-3 dBm)
// <i> Configure the minimum radiated TX power level for Bluetooth connections and DTM testing.
// <i> This configuration is used for power control on Bluetooth connections
// <i> if the LE Power Control feature is enabled.
// <i> When this configuration is passed into stack initialization, the stack
// <i> will select the closest value that the device supports.
// <i> API sl_bt_system_get_tx_power_setting() can be used to query the selected value.
#define SL_BT_CONFIG_MIN_TX_POWER     (-30)

// <o SL_BT_CONFIG_MAX_TX_POWER> Maximum radiated TX power level in 0.1dBm unit
// <i> Default: 80 (8 dBm)
// <i> Configure the maximum radiated TX power level for Bluetooth connections,
// <i> advertising, scanning and DTM testing.
// <i> When this configuration is passed into stack initialization, the stack
// <i> will select the closest value that the device supports.
// <i> API sl_bt_system_get_tx_power_setting() can be used to query the selected value.
#define SL_BT_CONFIG_MAX_TX_POWER     (80)

// </h> End TX Power Levels

// <h> RF Path

// <o SL_BT_CONFIG_RF_PATH_GAIN_TX> RF TX path gain in 0.1dBm unit
// <i> Default: 0
// <i> The Bluetooth stack takes TX RF path gain into account when adjusting transmitter
// <i> output power. Power radiated from the antenna then matches the application request.
// <i> For example, with radiated TX power set to +10 dBm and RF TX path
// <i> gain to -1 dBm, the transmitter output power will be set to +11 dBm.
#define SL_BT_CONFIG_RF_PATH_GAIN_TX     (0)

// <o SL_BT_CONFIG_RF_PATH_GAIN_RX> RF RX path gain in 0.1dBm unit
// <i> Default: 0
// <i> RX RF path gain is used to compensate the RSSI reports from the Bluetooth Stack.
#define SL_BT_CONFIG_RF_PATH_GAIN_RX     (0)

// </h> End RF Path

// <<< end of configuration section >>>
#if defined(SL_COMPONENT_CATALOG_PRESENT)
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_KERNEL_PRESENT
void sli_bt_rtos_ll_callback();
void sli_bt_rtos_stack_callback();
  #define  SL_BT_CONFIG_FLAGS         SL_BT_CONFIG_FLAG_RTOS
  #define SL_BT_CONFIG_LL_CALLBACK    sli_bt_rtos_ll_callback
  #define SL_BT_CONFIG_STACK_CALLBACK sli_bt_rtos_stack_callback
#else
  #define SL_BT_CONFIG_FLAGS          0
  #define SL_BT_CONFIG_LL_CALLBACK    0
  #define SL_BT_CONFIG_STACK_CALLBACK 0
#endif

extern const struct bg_gattdb_def bg_gattdb_data;

#include "sl_bt_stack_config.h"

#define SL_BT_CONFIG_DEFAULT                                                   \
  {                                                                            \
    .config_flags = SL_BT_CONFIG_FLAGS,                                        \
    .bluetooth.max_connections = SL_BT_CONFIG_MAX_CONNECTIONS,                 \
    .bluetooth.max_advertisers = SL_BT_CONFIG_MAX_ADVERTISERS,                 \
    .bluetooth.max_periodic_sync = SL_BT_CONFIG_MAX_PERIODIC_ADVERTISING_SYNC, \
    .bluetooth.max_buffer_memory = SL_BT_CONFIG_BUFFER_SIZE,                   \
    .scheduler_callback = SL_BT_CONFIG_LL_CALLBACK,                            \
    .stack_schedule_callback = SL_BT_CONFIG_STACK_CALLBACK,                    \
    .gattdb = &bg_gattdb_data,                                                 \
    .max_timers = SL_BT_CONFIG_MAX_SOFTWARE_TIMERS,                            \
    .rf.tx_gain = SL_BT_CONFIG_RF_PATH_GAIN_TX,                                \
    .rf.rx_gain = SL_BT_CONFIG_RF_PATH_GAIN_RX,                                \
    .rf.tx_min_power = SL_BT_CONFIG_MIN_TX_POWER,                              \
    .rf.tx_max_power = SL_BT_CONFIG_MAX_TX_POWER,                              \
  }

#endif // SL_BLUETOOTH_CONFIG_H
