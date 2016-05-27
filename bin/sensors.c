#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>

#include "ds18b20.h"
#include "missing.h"
#include "sensors.h"
#include "state.h"

// One sample:
struct sample {
	int32_t			celsius;	// Temp in degrees C * 10000
	enum ds18b20_status	status;		// Sensor status
};

// Sensor address table:
static const uint8_t sensors[][8] = {
	{ 0x28, 0x1C, 0xF0, 0x1E, 0x00, 0x00, 0x80, 0x3F },
	{ 0x28, 0x3A, 0x00, 0x03, 0x00, 0x00, 0x80, 0x38 },
	{ 0x28, 0xE9, 0xFF, 0x02, 0x00, 0x00, 0x80, 0xE3 },
	{ 0x28, 0x97, 0xCF, 0x1E, 0x00, 0x00, 0x80, 0xC6 },
	{ 0x28, 0x2A, 0x9B, 0x1E, 0x00, 0x00, 0x80, 0x01 },
	{ 0x28, 0x65, 0xD0, 0x1E, 0x00, 0x00, 0x80, 0xC9 },
	{ 0x28, 0x43, 0x87, 0x1E, 0x00, 0x00, 0x80, 0x09 },
};

// Size of sensor table:
#define NSENSORS	sizeof(sensors) / sizeof(sensors[0])

// What we want to do in this project is to wake up every 15 minutes, take a
// sample, retry the sample-taking a certain amount of times if we didn't get
// valid samples from all sensors, and consolidate the data into a record. Save
// this record to RTC memory, then go into deep sleep.
// Once every four wakeups (i.e. once an hour), take all records, consolidate
// them into a per-sensor hourly average, and push those over wifi.

// Sample table:
static struct sample samples[SENSORS_ROUNDS_MAX][NSENSORS];

// Consolidated sensor records:
static struct sample records[SENSORS_RECORDS_MAX][NSENSORS];

// Get size of one record (containing one sample round for all sensors)
uint8_t ICACHE_FLASH_ATTR
sensors_record_size (void)
{
	return sizeof(records[0]);
}

// Get a record
void * ICACHE_FLASH_ATTR
sensors_record_data (const uint8_t n)
{
	return records[n];
}

// Print sensor data in JSON format
size_t ICACHE_FLASH_ATTR
sensors_json (char *buf)
{
	char *p = buf;
	bool first = true;

	/* Create the following JSON structure:

		"sensors" : {
		  "sensor-id-0" : { "value" : "230000", "status" : "message" }
		, "sensor-id-1" : { "value" : "230000", "status" : "message" }
		}
	*/

	p += os_sprintf(p, "\"sensors\" : {\n");

	for (size_t sensor = 0; sensor < NSENSORS; sensor++) {
		const uint8_t *a = sensors[sensor];

		p += os_sprintf(p,
			"%s \"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\" : ",
			(first) ? " " : ",",
			a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);

		p += os_sprintf(p,
			"{ \"value\": \"%d\", \"status\" : \"%s\" }\n",
			records[0][sensor].celsius,
			ds18b20_status_string(records[0][sensor].status));

		first = false;
	}

	p += os_sprintf(p, "}");

	return p - buf;
}

// Called when the temperature conversion is done
static void ICACHE_FLASH_ATTR
on_timer (void *data)
{
	state_change(STATE_SENSORS_READOUT);
}

// Kickoff a timer to wait for the conversion to finish
static void ICACHE_FLASH_ATTR
timer_kickoff (void)
{
	static os_timer_t timer;

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, (os_timer_func_t *) on_timer, NULL);
	os_timer_arm(&timer, 800, 0);
}

// Consolidate multiple samples into one sample + one status
static void ICACHE_FLASH_ATTR
consolidate (struct sample samples[][NSENSORS], size_t nrounds, size_t sensor, struct sample *dest)
{
	int32_t sum   = 0;
	uint8_t count = 0;
	enum ds18b20_status max_status = DS18B20_UNPROBED;

	// Loop over all valid samples:
	for (size_t round = 0; round < nrounds; round++) {
		const struct sample *s = &samples[round][sensor];

		// Higher statuses are "more alive":
		if (s->status > max_status)
			max_status = s->status;

		// Sum all valid measurements:
		if (s->status == DS18B20_SUCCESS) {
			sum += s->celsius;
			count++;
		}
	}

	// Save average temperature and status:
	dest->celsius = (count > 0) ? sum / count : 0;
	dest->status  = max_status;
}

// Consolidate multiple records into one record per sensor
void ICACHE_FLASH_ATTR
sensors_consolidate_records (void)
{
	// Save average temperature and status into record #0:
	for (size_t sensor = 0; sensor < NSENSORS; sensor++)
		consolidate(records, SENSORS_RECORDS_MAX, sensor, &records[0][sensor]);
}

// Consolidate all sensors into destination record
void ICACHE_FLASH_ATTR
sensors_consolidate_samples (const size_t record)
{
	// Save average temperature and status into current record:
	for (size_t sensor = 0; sensor < NSENSORS; sensor++)
		consolidate(samples, SENSORS_ROUNDS_MAX, sensor, &records[record][sensor]);
}

// Check if sensor has at least one valid sample
static inline bool
sensor_has_sample (const size_t sensor)
{
	for (size_t round = 0; round < SENSORS_ROUNDS_MAX; round++)
		if (samples[round][sensor].status == DS18B20_SUCCESS)
			return true;

	return false;
}

// Check if all sensors have at least one valid sample
bool ICACHE_FLASH_ATTR
sensors_all_valid (void)
{
	for (size_t sensor = 0; sensor < NSENSORS; sensor++)
		if (!sensor_has_sample(sensor))
			return false;

	return true;
}

// Request temperature conversion
void ICACHE_FLASH_ATTR
sensors_request (const size_t round)
{
	// Kick off measurements:
	for (size_t sensor = 0; sensor < NSENSORS; sensor++)
		samples[round][sensor].status = ds18b20_request(sensors[sensor]);

	// Set wait timer:
	timer_kickoff();
}

// Get actual sensor reading, store into sensor table
void ICACHE_FLASH_ATTR
sensors_readout (const size_t round)
{
	for (size_t sensor = 0; sensor < NSENSORS; sensor++) {
		struct sample *sample = &samples[round][sensor];
		sample->status = ds18b20_result(sensors[sensor], &sample->celsius);
	}
}
