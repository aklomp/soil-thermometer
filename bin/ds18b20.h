// Sensor status flags. These are in a specific order, from "least alive" to
// "most alive". The first one should be zero to indicate "not probed yet".
enum ds18b20_status {
	DS18B20_UNPROBED = 0,		// Nothing known
	DS18B20_ERROR_BUS,		// Onewire bus error
	DS18B20_ERROR_SILENCE,		// No response from sensor
	DS18B20_ERROR_CHECKSUM,		// Data checksum invalid
	DS18B20_ERROR_RESET_VAL,	// After-reset reading of 85 degrees
	DS18B20_SUCCESS,		// Everything OK
};

enum ds18b20_status ds18b20_request (const uint8_t *addr);
enum ds18b20_status ds18b20_result  (const uint8_t *addr, int32_t *celsius);
const char *ds18b20_status_string   (const enum ds18b20_status);
