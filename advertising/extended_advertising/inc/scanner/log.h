#ifndef LOG_H
#define LOG_H

#ifdef LOCAL_LOG_OFF
#define GK_LOGD(_tag_, __fmt__, ...)
#define GK_LOGI(_tag_, __fmt__, ...)
#define GK_LOGW(_tag_, __fmt__, ...)
#define GK_LOGE(_tag_, __fmt__, ...)
#define GK_LOGV(_tag_, __fmt__, ...)
#define GK_UINT8_ARRAY_DUMP(array_base, array_size)
#define GK_ADDRESSING()
#define GK_CHECK(tag__, x)
#define LOG_ASSERT(x)
#define LOG(...)
#define LOGN()
#define UINT8_ARRAY_DUMP(array_base, array_size)
#define LOG_DIRECT_ERR(__fmt__, ...)
#define LOGE(__fmt__, ...)
#define LOGW(__fmt__, ...)
#define LOGI(__fmt__, ...)
#define LOGD(__fmt__, ...)
#define LOGV(__fmt__, ...)
#define HEX_DUMP(array_base, array_size)
#define HEX_DUMP_REVS(array_base, array_size)
#define ERROR_ADDRESSING()
#define INIT_LOG()
#define EVT_LOG_C(_evt_name_, _attached_, ...)
#define EVT_LOG_I(_evt_name_, _attached_, ...)
#define EVT_LOG_V(_evt_name_, _attached_, ...)
#define SE_CALL(x) \
  do {             \
    x;             \
  } while (0)
#define GENERAL_ERR_CHECK(x)

#else /* (LOCAL_LOG_OFF == 1) */

#include "stdio.h"

#define NO_LOG  0
#define LVL_ERROR 1
#define LVL_WARNING 2
#define LVL_INFO  3
#define LVL_DEBUG 4
#define LVL_VERBOSE 5

/*
 * PORT_VCOM - using printf to output, the real destination depends on where
 * it's retargeted.
 *
 * SEGGER_JLINK_VIEWER - using SEGGER_RTT_printf to terminal 0
 */
#define PORT_VCOM            (1U << 0)
#define SEGGER_JLINK_VIEWER  (1U << 1)
#define PORT_ALL  (PORT_VCOM | SEGGER_JLINK_VIEWER)

#ifndef GENERAL_SUCCESS
#define GENERAL_SUCCESS 0
#endif

/* General part - High level definitions */
#ifndef LOG_LEVEL
#define LOG_LEVEL LVL_VERBOSE
#endif

#ifndef LOG_PORT
#define LOG_PORT  (PORT_VCOM)
#endif

#if (LOG_PORT & SEGGER_JLINK_VIEWER)
#include "SEGGER_RTT.h"
#else
#define RTT_CTRL_RESET                "\x1B[0m"         // Reset to default colors
#define RTT_CTRL_CLEAR                "\x1B[2J \x1B[H"         // Clear screen, reposition cursor to top left
#define RTT_CTRL_TEXT_BRIGHT_RED      "\x1B[1;31m"
#define RTT_CTRL_TEXT_BRIGHT_YELLOW   "\x1B[1;33m"
#define RTT_CTRL_TEXT_BRIGHT_CYAN     "\x1B[1;36m"
#define RTT_CTRL_TEXT_BRIGHT_GREEN    "\x1B[1;32m"
#endif

// The expected format of the logging message is [LOGGING_LEVEL]<MODULE_NAME>: xxxx...
#define ASSERT_FLAG                     "[ASSERT] "
#define ERROR_FLAG                      "[E] "
#define WARNING_FLAG                    "[W] "
#define INFO_FLAG                       "[I] "
#define DEBUG_FLAG                      "[D] "
#define VERBOSE_FLAG                    "[V] "

#ifndef SUB_MODULE_NAME
#define SUB_MODULE_NAME                 ""
#endif

#define END_OF_LOG_HEADER               ":> "

/*
 * LOG prefix, formatted to be TAG + Sub module name + end flag
 * e.g. [E] main.c :> ...
 */
#define LOG_ASSERT_PREFIX               RTT_CTRL_TEXT_BRIGHT_RED ASSERT_FLAG "[Assert-assert] " SUB_MODULE_NAME END_OF_LOG_HEADER
#define LOG_ERROR_PREFIX                RTT_CTRL_TEXT_BRIGHT_RED ERROR_FLAG SUB_MODULE_NAME END_OF_LOG_HEADER
#define LOG_WARNING_PREFIX              RTT_CTRL_TEXT_BRIGHT_YELLOW WARNING_FLAG SUB_MODULE_NAME END_OF_LOG_HEADER
#define LOG_INFO_PREFIX                 RTT_CTRL_TEXT_BRIGHT_CYAN INFO_FLAG SUB_MODULE_NAME END_OF_LOG_HEADER
#define LOG_DEBUG_PREFIX                RTT_CTRL_TEXT_BRIGHT_GREEN DEBUG_FLAG SUB_MODULE_NAME END_OF_LOG_HEADER
#define LOG_VERBOSE_PREFIX              RTT_CTRL_RESET VERBOSE_FLAG SUB_MODULE_NAME END_OF_LOG_HEADER

/*
 * LOG(...) - The very basic logging function for all the output
 */
#if (LOG_PORT == SEGGER_JLINK_VIEWER)
#define LOG(...)                SEGGER_RTT_printf(0, __VA_ARGS__)
#elif (LOG_PORT == PORT_VCOM)
#define LOG(...)                printf(__VA_ARGS__)
#elif (LOG_PORT == PORT_ALL)
#define LOG(...)                       \
  do {                                 \
    printf(__VA_ARGS__);               \
    SEGGER_RTT_printf(0, __VA_ARGS__); \
  } while (0)
#else
#define LOG(...)
#endif

#define LOG_ASSERT_MSG() do { LOG(LOG_ASSERT_PREFIX "ASSERT ERROR at %s:%d\n", __FILE__, __LINE__); } while (0)
#define LOG_ASSERT(x) do { if (!x) { LOG_ASSERT_MSG(); } } while (0)

#ifndef HEX_ALIGN_SIZE
#define HEX_ALIGN_SIZE  30
#endif

#define HEX_DUMP(array_base, array_size)                                            \
  do {                                                                              \
    for (int i_log_exlusive = 0; i_log_exlusive < (array_size); i_log_exlusive++) { \
      LOG(((i_log_exlusive + 1) % HEX_ALIGN_SIZE) ? "%02x " : "%02x\n",             \
          ((char*)(array_base))[i_log_exlusive]); }                                 \
    LOG("\n");                                                                      \
  } while (0)
#define HEX_DUMP_REVS(array_base, array_size)                                       \
  do {                                                                              \
    for (int i_log_exlusive = 0; i_log_exlusive < (array_size); i_log_exlusive++) { \
      LOG(((i_log_exlusive + 1) % HEX_ALIGN_SIZE) ? "%02x " : "%02x\n",             \
          ((char*)(array_base))[array_size - i_log_exlusive - 1]); }                \
    LOG("\n");                                                                      \
  } while (0)

#define LOGN()   \
  do {           \
    LOG("\r\n"); \
  } while (0)

/* Direct way to output Error message, for log.c to use */
#define LOG_DIRECT_ERR(__fmt__, ...)                      \
  do {                                                    \
    LOG(RTT_CTRL_TEXT_BRIGHT_RED __fmt__, ##__VA_ARGS__); \
  } while (0)

/**
 * Different Levels of Logging System
 */

/* Error */
#define LOGE(__fmt__, ...)                          \
  do {                                              \
    if (LOG_LEVEL >=  LVL_ERROR) {                  \
      LOG(LOG_ERROR_PREFIX __fmt__, ##__VA_ARGS__); \
    }                                               \
  } while (0)

#define LOGEA(__fmt__, ...)                    \
  do {                                         \
    if (LOG_LEVEL >=  LVL_ERROR) {             \
      LOGE(__fmt__, ##__VA_ARGS__);            \
      LOGE("\t|--> File - %s, Line - %lu\r\n", \
           __FILE__, (unsigned long)__LINE__); \
    }                                          \
  } while (0)

/* Warning */
#define LOGW(__fmt__, ...)                            \
  do {                                                \
    if (LOG_LEVEL >=  LVL_WARNING) {                  \
      LOG(LOG_WARNING_PREFIX __fmt__, ##__VA_ARGS__); \
    }                                                 \
  } while (0)

/* Information */
#define LOGI(__fmt__, ...)                         \
  do {                                             \
    if (LOG_LEVEL >=  LVL_INFO) {                  \
      LOG(LOG_INFO_PREFIX __fmt__, ##__VA_ARGS__); \
    }                                              \
  } while (0)

/* DEBUG */
#define LOGD(__fmt__, ...)                          \
  do {                                              \
    if (LOG_LEVEL >=  LVL_DEBUG) {                  \
      LOG(LOG_DEBUG_PREFIX __fmt__, ##__VA_ARGS__); \
    }                                               \
  } while (0)

/* Verbose */
#define LOGV(__fmt__, ...)                            \
  do {                                                \
    if (LOG_LEVEL >=  LVL_VERBOSE) {                  \
      LOG(LOG_VERBOSE_PREFIX __fmt__, ##__VA_ARGS__); \
    }                                                 \
  } while (0)

/* Address error - file and line */
#define ERROR_ADDRESSING()                   \
  do {                                       \
    LOGE("\t|--> File - %s, Line - %lu\r\n", \
         __FILE__, (unsigned long)__LINE__); \
  } while (0)

#if (LOG_PORT == SEGGER_JLINK_VIEWER)
#define INIT_LOG()                                                                    \
  do {                                                                                \
    SEGGER_RTT_Init();                                                                \
    LOGI("--------- RTT LOG - Compiled - %s - %s ---------\r\n", __DATE__, __TIME__); \
  } while (0)
#elif (LOG_PORT == PORT_VCOM)
#define INIT_LOG()                                                                     \
  do {                                                                                 \
    LOGI("--------- VCOM LOG - Compiled - %s - %s ---------\r\n", __DATE__, __TIME__); \
  } while (0)
#elif (LOG_PORT == PORT_ALL)
#define INIT_LOG()                                                                         \
  do {                                                                                     \
    SEGGER_RTT_Init();                                                                     \
    LOGI("--------- RTT&VCOM LOG - Compiled - %s - %s ---------\r\n", __DATE__, __TIME__); \
  } while (0)
#else
#define INIT_LOG()
#endif

/*
 * NOTE: Below features are out of maintenance.
 */

/**
 * Event Log Part
 */
#define EVENT_LOG_LEVEL                   VERBOSE

#define NO_EVENT_LOG                      0
#define CRITICAL                          1
#define IMPORTANT                         2
#define VERBOSE                           3

#define CRITICAL_COLOR                    RTT_CTRL_TEXT_BRIGHT_RED
#define IMPORTANT_COLOR                   RTT_CTRL_TEXT_BRIGHT_YELLOW
#define VERBOSE_COLOR                     RTT_CTRL_RESET

#define FUNCTION                          ""
#define EVT_CATEGORY                      ""

/*FUNCTION Macro doesn't be used yet */
#define EVT_CRITICAL_PREFIX               CRITICAL_COLOR FUNCTION EVT_CATEGORY
#define EVT_IMPORTANT_PREFIX              IMPORTANT_COLOR FUNCTION EVT_CATEGORY
#define EVT_VERBOSE_PREFIX                VERBOSE_COLOR FUNCTION EVT_CATEGORY

#define LOG_MODULE_CLASS_COEX             1
#define LOG_MODULE_CLASS_DFU              1
#define LOG_MODULE_CLASS_ENDPOINT         1
#define LOG_MODULE_CLASS_PSTORE           1
#define LOG_MODULE_CLASS_GATT             1
#define LOG_MODULE_CLASS_GATT_SERVER      1
#define LOG_MODULE_CLASS_HARDWARE         1
#define LOG_MODULE_CLASS_LE_CONNECTION    1
#define LOG_MODULE_CLASS_LE_GAP           1
#define LOG_MODULE_CLASS_LE_SM            1
#define LOG_MODULE_CLASS_SYSTEM           1
#define LOG_MODULE_CLASS_TEST             1
#define LOG_MODULE_CLASS_USER             1

/**
 * Critical events
 */
#define EVT_LOG_C(_evt_name_, _attached_, ...)                       \
  do {                                                               \
    if (EVENT_LOG_LEVEL >= CRITICAL) {                               \
      LOG(EVT_CRITICAL_PREFIX _evt_name_ _attached_, ##__VA_ARGS__); \
    }                                                                \
  } while (0)

/**
 * Important events
 */
#define EVT_LOG_I(_evt_name_, _attached_, ...)                        \
  do {                                                                \
    if (EVENT_LOG_LEVEL >= IMPORTANT) {                               \
      LOG(EVT_IMPORTANT_PREFIX _evt_name_ _attached_, ##__VA_ARGS__); \
    }                                                                 \
  } while (0)

/**
 * Verbose events
 */
#define EVT_LOG_V(_evt_name_, _attached_, ...)                      \
  do {                                                              \
    if (EVENT_LOG_LEVEL >= VERBOSE) {                               \
      LOG(EVT_VERBOSE_PREFIX _evt_name_ _attached_, ##__VA_ARGS__); \
    }                                                               \
  } while (0)

/**
 * Test purpose
 */
#define TRY_OUT_ALL_COLORS()                                                                    \
  do {                                                                                          \
    for (uint8_t i_log_exlusive = 1; i_log_exlusive < 8; i_log_exlusive++) {                    \
      SEGGER_RTT_printf(0, "[2;3%dm" "Normal color. Test For Log out...\r\n", i_log_exlusive); \
      SEGGER_RTT_printf(0, "[1;3%dm" "Bright color. Test For Log out...\r\n", i_log_exlusive); \
    }                                                                                           \
  } while (0)

/**
 * Function declarations
 */
uint16_t error_checking(uint16_t error_code, uint8_t directly);
void log_events(const void *v_evt);

#define SE_CALL(x) \
  do {             \
    if ((x) != 0) { ERROR_ADDRESSING(); } } while (0)

#define SURROUNDING(x)          "<" x "> - "

#define GENERAL_ERR_CHECK(x)                            \
  do {                                                  \
    if (x != GENERAL_SUCCESS) {                         \
      LOGE("Error Return. Error code = 0x%04x\r\n", x); \
      ERROR_ADDRESSING();                               \
    }                                                   \
  } while (0)

#endif
#endif
