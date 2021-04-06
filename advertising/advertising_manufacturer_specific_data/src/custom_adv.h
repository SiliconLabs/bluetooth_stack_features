#ifndef _CUSTOM_ADV_H_
#define _CUSTOM_ADV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sl_bt_api.h"
#include "sl_app_assert.h"

#define NAME_MAX_LENGTH 20

typedef struct
{
	uint8_t len_flags;
	uint8_t type_flags;
	uint8_t val_flags;

	uint8_t len_manuf;
	uint8_t type_manuf;
	// First two bytes must contain the manufacturer ID (little-endian order) 
	uint8_t company_LO;
	uint8_t company_HI;

	// The next bytes are freely configurable - using one byte for counter value and one byte for last button press 
	uint8_t num_presses;
	uint8_t last_press;

	// length of the name AD element is variable, adding it last to keep things simple 
	uint8_t len_name;
	uint8_t type_name;

  // NAME_MAX_LENGTH must be sized so that total length of data does not exceed 31 bytes
	char name[NAME_MAX_LENGTH]; 

	// These values are NOT included in the actual advertising payload, just for bookkeeping 
	char dummy;        // Space for null terminator
	uint8_t data_size; // Actual length of advertising data
} CustomAdv_t;

void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID, uint8_t num_presses, uint8_t last_press, char *name);
void start_adv(CustomAdv_t *pData, uint8_t advertising_set_handle);
void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle, uint8_t num_presses, uint8_t last_press);

#ifdef __cplusplus
}
#endif

#endif
