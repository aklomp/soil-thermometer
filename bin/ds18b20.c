#include <os_type.h>
#include <osapi.h>

#include "ds18b20.h"
#include "missing.h"
#include "onewire.h"

#define CMD_MATCH_ROM	0x55
#define CMD_CONVERSION	0x44
#define CMD_GET_RESULT	0xBE

const char * ICACHE_FLASH_ATTR
ds18b20_status_string (const enum ds18b20_status status)
{
	switch (status) {
	case DS18B20_UNPROBED:		return "unprobed";
	case DS18B20_ERROR_BUS:		return "bus error";
	case DS18B20_ERROR_SILENCE:	return "no response";
	case DS18B20_ERROR_CHECKSUM:	return "checksum error";
	case DS18B20_ERROR_RESET_VAL:	return "reset value";
	case DS18B20_SUCCESS:		return "success";
	}
}

static bool ICACHE_FLASH_ATTR
check_crc (const uint8_t *data)
{
	uint8_t crc = 0;

	for (uint8_t i = 8; i; i--) {
		uint8_t d = *data++;
		for (uint8_t j = 8; j; j--, d >>= 1)
			crc = ((crc ^ d) & 1)
				? (crc >> 1) ^ 0x8C
				: (crc >> 1);
	}

	return (crc == *data);
}

// Select a specific sensor
static void ICACHE_FLASH_ATTR
select (const uint8_t *addr)
{
	// Send the "Match ROM" command:
	onewire_write(CMD_MATCH_ROM);

	// Send requested address:
	for (const uint8_t *c = addr; c < addr + 8; c++)
		onewire_write(*c);
}

// Request a temperature conversion
enum ds18b20_status ICACHE_FLASH_ATTR
ds18b20_request (const uint8_t *addr)
{
	// Resetting the bus fails if no presence is signaled:
	if (!onewire_reset())
		return DS18B20_ERROR_BUS;

	// Select and issue "convert temperature" command:
	select(addr);
	onewire_write(CMD_CONVERSION);
	return DS18B20_SUCCESS;
}

static inline enum ds18b20_status
print_status (enum ds18b20_status status)
{
	os_printf("%s\n", ds18b20_status_string(status));
	return status;
}

// Return the result of a temperature conversion
enum ds18b20_status ICACHE_FLASH_ATTR
ds18b20_result (const uint8_t *addr, int32_t *celsius)
{
	union {
		uint8_t		c[9];	// All data bytes
		int16_t		t[1];	// Temperature data
		uint32_t	l[2];	// First 8 bytes
	} data;

	// Start log line:
	os_printf("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x: ",
		addr[0], addr[1], addr[2], addr[3],
		addr[4], addr[5], addr[6], addr[7]);

	// Resetting the bus fails if no presence is signaled:
	if (!onewire_reset())
		return print_status(DS18B20_ERROR_BUS);

	// Select and issue "read scratchpad" command:
	select(addr);
	onewire_write(CMD_GET_RESULT);

	// Read data bytes:
	for (uint8_t i = 0; i < sizeof(data.c); i++)
		data.c[i] = onewire_read();

	// If all data bytes are 0xFF, nobody responded:
	if (data.l[0] == 0xFFFFFFFF && data.l[1] == 0xFFFFFFFF)
		return print_status(DS18B20_ERROR_SILENCE);

	// Check CRC:
	if (!check_crc(data.c))
		return print_status(DS18B20_ERROR_CHECKSUM);

	// Temperature measurement is given in 1/16 degrees C,
	// convert to degrees * 10000:
	*celsius = (data.t[0] * 10000) / 16;

	// Reading 85 degrees means we've got the reset value:
	if (*celsius == 850000)
		return print_status(DS18B20_ERROR_RESET_VAL);

	// Satisfy our curiosity:
	os_printf("%d\n", *celsius);

	return DS18B20_SUCCESS;
}
