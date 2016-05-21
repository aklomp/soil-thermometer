#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>
#include <gpio.h>

#include "missing.h"

// Pin 5 is the power pin (D1 on the NodeMCU)
// Pin 4 is the data pin (D2 on the NodeMCU)
#define PIN_POWER	5
#define PIN_DATA	4

static inline void
line_low (void)
{
	GPIO_OUTPUT_SET(PIN_DATA, 0);
}

static inline void
line_release (void)
{
	GPIO_DIS_OUTPUT(PIN_DATA);
}

static inline bool
line_read (void)
{
	return GPIO_INPUT_GET(PIN_DATA);
}

// Write a byte
void ICACHE_FLASH_ATTR
onewire_write (const uint8_t c)
{
	uint8_t delays[2][2] = {
		{ 58, 6 },		// 0-bit
		{ 8, 56 },		// 1-bit
	};

	for (uint8_t i = 0; i < 8; i++) {
		uint8_t *delay = delays[(c >> i) & 1];

		ets_intr_lock();

		line_low();
		os_delay_us(delay[0]);

		line_release();
		os_delay_us(delay[1]);

		ets_intr_unlock();
	}
}

// Read a byte
uint8_t ICACHE_FLASH_ATTR
onewire_read (void)
{
	uint8_t c = 0;

	for (uint8_t mask = 1; mask; mask <<= 1) {

		ets_intr_lock();

		// Start bit by pulling line down briefly:
		line_low();
		os_delay_us(5);

		// Give slave some time to respond:
		line_release();
		os_delay_us(10);

		// Sample:
		if (line_read())
			c |= mask;

		// Wait for read slot to finish:
		os_delay_us(50);

		ets_intr_unlock();
	}

	return c;
}

// Send reset signal on 1-wire bus
bool ICACHE_FLASH_ATTR
onewire_reset (void)
{
	bool ret = true;

	// Pull line low for > 480us:
	line_low();
	os_delay_us(500);

	// Release line, wait for slave to respond:
	line_release();

	// Check that line is pulled down by slave:
	os_delay_us(100);
	if (line_read()) {
		os_printf("%s: slave not pulling down line\n", __FUNCTION__);
		ret = false;
	}

	// Wait for slave to release line:
	os_delay_us(500);
	if (!line_read()) {
		os_printf("%s: line not pulled up\n", __FUNCTION__);
		ret = false;
	}

	return ret;
}

// Depower the power line
void ICACHE_FLASH_ATTR
onewire_depower (void)
{
	GPIO_OUTPUT_SET(PIN_POWER, 0);
}

// Initialize 1-wire bus
void ICACHE_FLASH_ATTR
onewire_init (void)
{
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);

	// Pull the power pin high:
	GPIO_OUTPUT_SET(PIN_POWER, 1);
}
