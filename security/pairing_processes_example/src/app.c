/***************************************************************************/ /**
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
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "gatt_db.h"
#include "app.h"
#include "sl_simple_button_instances.h"
#include "sl_simple_led_instances.h"
#include "app_log.h"
#include "dmd.h"
#include "glib.h"
#include "sl_board_control.h"
#include "stdio.h"

#define DISPLAYONLY 0
#define DISPLAYYESNO 1
#define KEYBOARDONLY 2
#define NOINPUTNOOUTPUT 3
#define KEYBOARDDISPLAY 4

#define IO_CAPABILITY (KEYBOARDONLY) // Choose IO capabilities.
#define MITM_PROTECTION (0x01)      // 0=JustWorks, 1=PasskeyEntry or NumericComparison

#ifndef IO_CAPABILITY
#error "You must define which IO capabilities device supports"
#endif

#if (IO_CAPABILITY == KEYBOARDONLY)
#define KEYBOARD_ONLY_PASSKEY (101011)
#else
#define KEYBOARD_ONLY_PASSKEY (000001)
#endif

// Configuration defines
#define X_BORDER 5
#define Y_BORDER 2
typedef enum {
  IDLE,
  DISPLAY_PASSKEY,
  PROMPT_YESNO,
  PROMPT_CONFIRM_SUBMIT_PASSKEY,
  PROMPT_INPUTTING_PASSKEY,
  BOND_SUCCESS,
  BOND_FAILURE
} State_t;

typedef enum {
  REJECT_COMPARISON,
  ACCEPT_COMPARISON,
  REENTER_PASSKEY,
  SUBMIT_PASSKEY,
  DISPLAY_REFRESH
} Signal_t;

#define SCREEN_REFRESH_PERIOD (32768 / 4)

// Secure service 0b282ff4-5347-472b-93da-f579103420fa
// Public characteristic d566b326-a76f-4891-9e04-3c143eefad85
// Encrypted characteristic 8824e363-7392-4bfc-81b6-3e58104cb2b0
// Authenticated characteristic b6be53b3-d3fd-4f9e-a22b-bc5d9dc58c6a

static const uint8_t SERVICE_UUID[16] = { 0xfa, 0x20, 0x34, 0x10, 0x79, 0xf5, 0xda, 0x93, 0x2b, 0x47, 0x47, 0x53, 0xf4, 0x2f, 0x28, 0x0b };

/*static const uint8_t PUBLIC_CHARACTERISTIC_UUID[16] = {0x85, 0xad, 0xef, 0x3e, 0x14, 0x3c, 0x04, 0x9e, 0x91, 0x48, 0x6f, 0xa7, 0x26, 0xb3, 0x66, 0xd5};

   static const uint8_t ENCRYPTED_CHARACTERISTIC_UUID[16] = {0xb0, 0xb2, 0x4c, 0x10, 0x58, 0x3e, 0xb6, 0x81, 0xfc, 0x4b, 0x92, 0x73, 0x63, 0xe3, 0x24, 0x88};

   static const uint8_t AUTHENTICATED_CHARACTERISTIC_UUID[16] = {0x6a, 0x8c, 0xc5, 0x9d, 0x5d, 0xbc, 0x2b, 0xa2, 0x9e, 0x4f, 0xfd, 0xd3, 0xb3, 0x53, 0xbe, 0xb6};
 */

// Character constants
static const char DIGIT_CHOICES[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
const char INITIATOR_STRING[] = "INITIATOR";
const char RESPONDER_STRING[] = "RESPONDER";

// Pointers and defaults for strings to manipulate on display
static char passkey_digits[] = "000000";
static char role_display_string[] = "   INITIATOR   ";
static char passkey_display_string[] = "    000000";
static char cursor_string[] = "    ^     ";

static const char *digit_choice_ptr = &DIGIT_CHOICES[0];
const char *role_string_ptr = INITIATOR_STRING;
static char *current_char_ptr = &passkey_digits[0];
static char *cursor_ptr = &cursor_string[4];

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

static uint8_t connection = 0xFF;
static volatile bool is_initiator = true;
static volatile uint32_t passkey = 0;
static volatile State_t state = IDLE;
// Globals
static uint32_t xOffset, yOffset;
static GLIB_Context_t glibContext;

sl_sleeptimer_timer_handle_t sleep_timer_handle;
void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data);

static void setup_advertising_or_scanning(void);
#if (IO_CAPABILITY != KEYBOARDONLY)
static uint32_t make_passkey_from_address(bd_addr address);
#endif
static bool process_scan_response_for_uuid(sl_bt_evt_scanner_legacy_advertisement_report_t *pResp);
void graphics_init(void);
static void print_empty_lines(uint8_t N);
static void refresh_display(void);
void graphics_clear(void);
/**************************************************************************/ /**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  // Initialize display
  graphics_init();
  if (sl_button_get_state(&sl_button_btn0)) {
    // Initiator
    is_initiator = false;
    role_string_ptr = RESPONDER_STRING;
  } else {
    is_initiator = true;
    role_string_ptr = INITIATOR_STRING;
  }
}

/**************************************************************************/ /**
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

/**************************************************************************/ /**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  bd_addr address;
  uint8_t address_type;
  uint8_t system_id[8];

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:

      app_log_info("Stack version: %u.%u.%u\r\r\n",
                   evt->data.evt_system_boot.major,
                   evt->data.evt_system_boot.minor,
                   evt->data.evt_system_boot.patch);

      state = IDLE;
      connection = 0xFF;
      // Extract unique ID from BT Address.
      sc = sl_bt_system_get_identity_address(&address, &address_type);
      app_assert_status(sc);

      /* Print Bluetooth address */
      app_log_info("Bluetooth %s address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   address_type ? "static random" : "public device",
                   address.addr[5],
                   address.addr[4],
                   address.addr[3],
                   address.addr[2],
                   address.addr[1],
                   address.addr[0]);
      // Pad and reverse unique ID to get System ID.
      system_id[0] = address.addr[5];
      system_id[1] = address.addr[4];
      system_id[2] = address.addr[3];
      system_id[3] = 0xFF;
      system_id[4] = 0xFE;
      system_id[5] = address.addr[2];
      system_id[6] = address.addr[1];
      system_id[7] = address.addr[0];

      sc = sl_bt_gatt_server_write_attribute_value(gattdb_system_id,
                                                   0,
                                                   sizeof(system_id),
                                                   system_id);
      app_assert_status(sc);

      // Configuration according to constants set at compile time.
      sc = sl_bt_sm_configure(MITM_PROTECTION, IO_CAPABILITY);
      app_assert_status(sc);
#if (IO_CAPABILITY == KEYBOARDONLY)
      // If KeyboardOnly, prepare for "Both input" case initially.
      passkey = KEYBOARD_ONLY_PASSKEY;
#else
      passkey = make_passkey_from_address(address);
#endif
      sc = sl_bt_sm_set_passkey(passkey);
      app_assert_status(sc);
      sc = sl_bt_sm_set_bondable_mode(1);
      app_assert_status(sc);
      app_log_info("Boot event - %s\r\n",
                   is_initiator ? "Starting advertising" : "Starting discovery");

      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      app_log_info("All bonding deleted\r\n");
      setup_advertising_or_scanning();

      // Start timer to refresh display
      sc = sl_sleeptimer_start_periodic_timer(&sleep_timer_handle, SCREEN_REFRESH_PERIOD, sleeptimer_callback, (void*)NULL, 0, 0);
      app_assert_status(sc);
      break;

    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      if (process_scan_response_for_uuid(&evt->data.evt_scanner_legacy_advertisement_report)) {
        sc = sl_bt_scanner_stop();
        app_assert_status(sc);
        app_log_info("Connecting to device at address: ");

        for (int i = 0; i < 5; i++) {
          app_log_info("%2.2x:", evt->data.evt_scanner_legacy_advertisement_report.address.addr[5 - i]);
        }
        app_log_info("%2.2x\r\n", evt->data.evt_scanner_legacy_advertisement_report.address.addr[0]);
        sc = sl_bt_connection_open(evt->data.evt_scanner_legacy_advertisement_report.address,
                                   evt->data.evt_scanner_legacy_advertisement_report.address_type,
                                   sl_bt_gap_1m_phy,
                                   &connection);
        app_assert_status(sc);
      }
      break;
    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      connection = evt->data.evt_connection_opened.connection;
      if (is_initiator) {
        sc = sl_bt_sm_increase_security(connection);
        app_assert_status(sc);
      }
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("Connection closed, reason: 0x%2.2x\r\n",
                   evt->data.evt_connection_closed.reason);
      sc = sl_bt_sm_delete_bondings();
      app_assert_status(sc);
      app_log_info("All bonding deleted\r\n");
      connection = 0xFF;
      state = IDLE;
      // Restart advertising after client has disconnected.
      setup_advertising_or_scanning();
      break;

    case sl_bt_evt_connection_parameters_id:

      switch (evt->data.evt_connection_parameters.security_mode) {
        case sl_bt_connection_mode1_level1:
          app_log_info("No Security\r\n");
          break;
        case sl_bt_connection_mode1_level2:
          app_log_info("Unauthenticated pairing with encryption (Just Works)\r\n");
          break;
        case sl_bt_connection_mode1_level3:
          app_log_info("Authenticated pairing with encryption (Legacy Pairing)\r\n");
          break;
        case sl_bt_connection_mode1_level4:
          app_log_info("Authenticated Secure Connections pairing with encryption (BT 4.2 LE Secure Pairing)\r\n");
          break;
        default:
          break;
      }
      break;

    case sl_bt_evt_sm_passkey_display_id:
      // Display passkey
      app_log_info("Passkey: %4lu\r\n", evt->data.evt_sm_passkey_display.passkey);
      passkey = evt->data.evt_sm_passkey_display.passkey;
      state = DISPLAY_PASSKEY;
      break;

    case sl_bt_evt_sm_passkey_request_id:
      app_log_info("Passkey request\r\n");
      state = PROMPT_INPUTTING_PASSKEY;
      break;

    case sl_bt_evt_sm_confirm_passkey_id:
      app_log_info("Passkey confirm\r\n");
      passkey = evt->data.evt_sm_confirm_passkey.passkey;
      state = PROMPT_YESNO;
      break;

    case sl_bt_evt_sm_confirm_bonding_id:
      app_log_info("Bonding confirm\r\n");
      sc = sl_bt_sm_bonding_confirm(evt->data.evt_sm_confirm_bonding.connection, 1);
      app_assert_status(sc);
      break;

    case sl_bt_evt_sm_bonded_id:
      app_log_info("Bond success\r\n");
      state = BOND_SUCCESS;
      break;

    case sl_bt_evt_sm_bonding_failed_id:
      app_log_info("Bonding failed, reason 0x%2X\r\n",
                   evt->data.evt_sm_bonding_failed.reason);
      sc = sl_bt_connection_close(evt->data.evt_sm_bonding_failed.connection);
      app_assert_status(sc);
      state = BOND_FAILURE;
      break;

    case sl_bt_evt_system_external_signal_id:
      switch (evt->data.evt_system_external_signal.extsignals) {
        case REJECT_COMPARISON:
          app_log_info("Rejecting prompt.\r\n");
          sc = sl_bt_sm_passkey_confirm(connection, 0);
          app_assert_status(sc);
          break;
        case ACCEPT_COMPARISON:
          app_log_info("Accepting prompt.\r\n");
          sc = sl_bt_sm_passkey_confirm(connection, 1);
          app_assert_status(sc);
          break;
        case REENTER_PASSKEY:
          app_log_info("Still fixing the passkey.\r\n");
          break;
        case SUBMIT_PASSKEY:
          app_log_info("Submitting passkey...\r\n");
          char *_temp;
          passkey = strtoul(passkey_digits, &_temp, 10);
          sc = sl_bt_sm_enter_passkey(connection, passkey);
          app_assert_status(sc);
          break;
        case DISPLAY_REFRESH:
          refresh_display();
          break;
        default:
          break;
      }
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

void graphics_init(void)
{
  EMSTATUS status;

  sl_board_enable_display();
  /* Initialize the DMD module for the DISPLAY device driver. */
  status = DMD_init(0);
  if (DMD_OK != status) {
    while (1) ;
  }

  status = GLIB_contextInit(&glibContext);
  if (GLIB_OK != status) {
    while (1) ;
  }

  glibContext.backgroundColor = White;
  glibContext.foregroundColor = Black;

  /* Use Normal font */
  GLIB_setFont(&glibContext, (GLIB_Font_t *)&GLIB_FontNormal8x8);
}

/**************************************************************************//**
 * @brief This function draws the initial display screen
 *****************************************************************************/
void GRAPHICS_Update(void)
{
  DMD_updateDisplay();
}

void graphics_clear(void)
{
  GLIB_clear(&glibContext);

  // Reset the offset values to their defaults
  xOffset = X_BORDER;
  yOffset = Y_BORDER;
}

void GRAPHICS_AppendString(char *str)
{
  GLIB_drawString(&glibContext,
                  str,
                  strlen(str),
                  xOffset,
                  yOffset,
                  0);
  yOffset += glibContext.font.fontHeight + glibContext.font.lineSpacing;
}
/**
 * @brief setup_adv_scan
 * Start advertising or scanning depending on the role.
 * Let GAP slave be the initiator, so the smartphone can be used easily.
 */
static void setup_advertising_or_scanning(void)
{
  sl_status_t sc;
  if (is_initiator) {
    // 1M PHY Advertising
    // Create an advertising set.
    sc = sl_bt_advertiser_create_set(&advertising_set_handle);
    app_assert_status(sc);

    // Set advertising interval to 100ms.
    sc = sl_bt_advertiser_set_timing(
      advertising_set_handle,
      160,   // min. adv. interval (milliseconds * 1.6)
      160,   // max. adv. interval (milliseconds * 1.6)
      0,     // adv. duration
      0);    // max. num. adv. events
    app_assert_status(sc);

    sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                               sl_bt_advertiser_general_discoverable);
    app_assert_status(sc);
    // Start general advertising and enable connections.
    sc = sl_bt_legacy_advertiser_start(
      advertising_set_handle,
      sl_bt_advertiser_connectable_scannable);
    app_assert_status(sc);
  } else {
    // Start scanning
    sc = sl_bt_scanner_start(sl_bt_gap_1m_phy, sl_bt_scanner_discover_generic);
    app_assert_status(sc);
  }
}

#if (IO_CAPABILITY != KEYBOARDONLY)
static uint32_t make_passkey_from_address(bd_addr address)
{
  uint32_t _passkey = 0;

  for (uint8_t i = 0; i < sizeof(bd_addr); i++) {
    _passkey += (address.addr[i] << 8);
  }
  return _passkey;
}
#endif

/**
 * @brief process_scan_response
 * Processes advertisement packets looking for custom service UUID.
 * @param pResp - scan_response structure passed in from scan_response event
 * @return adMatchFound - bool flag, 1 if correct service found
 */
static bool process_scan_response_for_uuid(sl_bt_evt_scanner_legacy_advertisement_report_t *pResp)
{
  int i = 0;
  bool ad_match_found = 0;
  int ad_len;
  int ad_type;

  while (i < (pResp->data.len - 1)) {
    ad_len = pResp->data.data[i];
    ad_type = pResp->data.data[i + 1];

    if (ad_type == 0x06 || ad_type == 0x07) {
      // Type 0x06 = Incomplete List of 128-bit Service Class UUIDs
      // Type 0x07 = Complete List of 128-bit Service Class UUIDs
      if (memcmp(SERVICE_UUID, &(pResp->data.data[i + 2]), 16) == 0) {
        app_log_info("Found Secure Service \r\n");
        ad_match_found = 1;
      }
    }
    // Jump to next AD record
    i = i + ad_len + 1;
  }
  return ad_match_found;
}

/**************************************************************************/ /**
 * @brief Prints N empty lines on the display
 *****************************************************************************/
static void print_empty_lines(uint8_t N)
{
  for (uint8_t ii = 0; ii < N; ++ii) {
    GRAPHICS_AppendString("");
  }
}

static void refresh_display(void)
{
  // Effective size about 12 lines by 15 chars
  graphics_clear();

  sprintf(&role_display_string[3], "%s", role_string_ptr);
  GRAPHICS_AppendString(role_display_string);
  print_empty_lines(5);

  switch (state) {
    case IDLE:
      break;
    case DISPLAY_PASSKEY:
      // Display key for inputting on other device
      sprintf(&passkey_display_string[4], "%lu", passkey);
      GRAPHICS_AppendString(passkey_display_string);
      break;
    case PROMPT_YESNO:
      // Display key for numeric comparison
      sprintf(&passkey_display_string[4], "%lu", passkey);
      GRAPHICS_AppendString(passkey_display_string);
      print_empty_lines(3);
      GRAPHICS_AppendString("YES         NO");
      break;
    case PROMPT_CONFIRM_SUBMIT_PASSKEY:
      // Display current entered passkey try and prompt to submit
      sprintf(&passkey_display_string[4], "%s", passkey_digits);
      GRAPHICS_AppendString(passkey_display_string);
      print_empty_lines(3);
      GRAPHICS_AppendString("YES         NO");
      break;
    case PROMPT_INPUTTING_PASSKEY:
      // Prompt to enter passkey attempt with cursor for moving
      sprintf(&passkey_display_string[4], "%s", passkey_digits);
      GRAPHICS_AppendString(passkey_display_string);
      GRAPHICS_AppendString(cursor_string);
      break;
    case BOND_SUCCESS:
      GRAPHICS_AppendString(" Bond success");
      break;
    case BOND_FAILURE:
      GRAPHICS_AppendString(" Bond failure");
      break;
    default:
      break;
  }

  DMD_updateDisplay();
}

/**************************************************************************/ /**
 * Simple Button
 * Button state changed callback
 * @param[in] handle Button event handle
 *****************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  // Button pressed.
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
    if (&sl_button_btn0 == handle) {
      // PB0 pressed down
      if (state == PROMPT_INPUTTING_PASSKEY) {
        if (current_char_ptr == &passkey_digits[5]) {
          // Wrap-around from last index
          // Prompt yes or no to submit current passkey
          state = PROMPT_CONFIRM_SUBMIT_PASSKEY;
          *current_char_ptr = *digit_choice_ptr;
          current_char_ptr = &passkey_digits[0];
          *cursor_ptr = ' ';
          cursor_ptr = &cursor_string[4];
          *cursor_ptr = '^';
        } else {
          *current_char_ptr++ = *digit_choice_ptr;
          *cursor_ptr++ = ' ';
          *cursor_ptr = '^';
        }
        digit_choice_ptr = &DIGIT_CHOICES[*current_char_ptr - '0'];
      } else if (state == PROMPT_YESNO) {
        // NO to comparison.
        sl_bt_external_signal(REJECT_COMPARISON);
      } else if (state == PROMPT_CONFIRM_SUBMIT_PASSKEY) {
        // NO to submit current entry for passkey.
        // Back to correcting the entry
        state = PROMPT_INPUTTING_PASSKEY;
        sl_bt_external_signal(REENTER_PASSKEY);
      }
      sl_led_toggle(&sl_led_led0);
    } else if (&sl_button_btn1 == handle) {
      if (state == PROMPT_INPUTTING_PASSKEY) {
        // Set digit pointer to same char as on display currently
        digit_choice_ptr = &DIGIT_CHOICES[*current_char_ptr - '0'];
        // Wrap-around 9 -> 0 and ask for submit
        if (*digit_choice_ptr == '9') {
          digit_choice_ptr = &DIGIT_CHOICES[0];
        } else {
          digit_choice_ptr++;
        }
        // Update digit to passcode string
        *current_char_ptr = *digit_choice_ptr;
      } else if (state == PROMPT_YESNO) {
        // YES to comparison
        sl_bt_external_signal(ACCEPT_COMPARISON);
      } else if (state == PROMPT_CONFIRM_SUBMIT_PASSKEY) {
        // YES to submit current entry for passkey.
        sl_bt_external_signal(SUBMIT_PASSKEY);
      }
      sl_led_toggle(&sl_led_led0);
    }
  }
}

/***************************************************************************//**
 * Sleeptimer callback
 *
 * Note: This function is called from interrupt context
 *
 * @param[in] handle Handle of the sleeptimer instance
 * @param[in] data  Callback data
 ******************************************************************************/
void sleeptimer_callback(sl_sleeptimer_timer_handle_t *handle, void *data)
{
  (void)handle;
  (void)data;

  sl_bt_external_signal(DISPLAY_REFRESH);
}
