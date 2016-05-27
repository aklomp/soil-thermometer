#include <os_type.h>
#include <osapi.h>
#include <user_interface.h>

#include "missing.h"
#include "sensors.h"

// Header structure for RTC memory block:
struct header {
	uint32_t	sig;
	uint8_t		num_records;
	uint8_t		record_size;
};

#define RECORD_SIG	0xDEADBEEF

// Round upwards to next 4 bytes:
#define ROUNDUP(x)	(((x) + 3) & ~0x03)

// Memory block address of n'th sensor record block:
#define RECORDADDR(n)	(64 + ROUNDUP(sizeof(struct header)) / 4 \
			+ (n) * (ROUNDUP(sensors_record_size()) / 4))

// Import RTC memory, return number of valid records:
uint8_t ICACHE_FLASH_ATTR
rtc_mem_load (void)
{
	struct header header;

	// Read header structure:
	if (!system_rtc_mem_read(64, &header, sizeof(header))) {
		os_printf("%s: read failed!\n", __FUNCTION__);
		goto err;
	}

	// Check signature:
	if (header.sig != RECORD_SIG) {
		os_printf("%s: signature fail!\n", __FUNCTION__);
		goto err;
	}

	// Check that record size is what we expect:
	if (header.record_size != sensors_record_size()) {
		os_printf("%s: record size: expected %u, got %u\n",
			__FUNCTION__, sensors_record_size(), header.record_size);
		goto err;
	}

	// Everything looks OK, let's import the existing records into the
	// sensors module:
	for (uint8_t i = 0; i < header.num_records; i++)
		if (!system_rtc_mem_read(RECORDADDR(i),
				sensors_record_data(i),
				sensors_record_size())) {
			os_printf("%s: read failed!\n", __FUNCTION__);
			goto err;
		}

	os_printf("%s: read %u records\n", __FUNCTION__, header.num_records);
	return header.num_records;

err:	memset(&header, 0, sizeof(header));
	return 0;
}

// Export header and sensor data to RTC memory
bool ICACHE_FLASH_ATTR
rtc_mem_save (uint8_t num_records)
{
	struct header header = {
		.sig		= RECORD_SIG,
		.num_records	= num_records,
		.record_size	= sensors_record_size()
	};

	// Write header:
	if (!system_rtc_mem_write(64, &header, sizeof(header)))
		goto err;

	// Write records:
	for (uint8_t i = 0; i < num_records; i++)
		if (!system_rtc_mem_write(RECORDADDR(i),
				sensors_record_data(i),
				header.record_size))
			goto err;

	os_printf("%s: wrote %u records\n", __FUNCTION__, num_records);
	return true;

err:	os_printf("%s: write failed!\n", __FUNCTION__);
	return false;
}
