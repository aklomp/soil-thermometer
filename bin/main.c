#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>

#include "ds18b20.h"
#include "http.h"
#include "led.h"
#include "missing.h"
#include "net.h"
#include "onewire.h"
#include "rtc_mem.h"
#include "sensors.h"
#include "state.h"
#include "uart.h"
#include "wifi.h"

#define NUM_EVENTS	4
#define DEEP_SLEEP_SEC	900UL
#define DEEP_SLEEP_USEC	(DEEP_SLEEP_SEC * 1000000UL)

// We wake up every 15 minutes, take temperature samples, and go to deep sleep.
// Every hour, we collect and consolidate the 15-minute samples into one final
// measurement that we send over wifi. We store our state inside the RTC clock
// memory, so we know at which step we are. Wakeup number:
static uint8_t wakeup;

static void ICACHE_FLASH_ATTR
deep_sleep (void)
{
	// Don't remember RF config across deep sleep:
	if (!system_deep_sleep_set_option(2))
		os_printf("Deep sleep: couldn't set option!\n");

	// Enter deep sleep:
	os_printf("Deep sleep: starting for %u sec\n", DEEP_SLEEP_SEC);
	system_deep_sleep(DEEP_SLEEP_USEC);
}

// Sensor event handler
static bool ICACHE_FLASH_ATTR
sensor_event (os_event_t *event)
{
	static uint8_t round = 0;

	switch (event->sig)
	{
	// Request sensor measurements:
	case STATE_SENSORS_START:
		led_blink(50);
		os_printf("Requesting sensor measurements\n");
		sensors_request(round);
		return true;

	// Obtain sensor measurements:
	case STATE_SENSORS_READOUT:
		sensors_readout(round);
		state_change(STATE_SENSORS_DONE);
		return true;

	// Sensor measurements obtained:
	case STATE_SENSORS_DONE:
		os_printf("Sensor measurements obtained\n");

		// If we don't have valid measurements for each sensor, retry:
		if (!sensors_all_valid() && ++round < SENSORS_ROUNDS_MAX) {
			os_printf("Retrying measurements (%d)\n", round);
			state_change(STATE_SENSORS_START);
			return true;
		}

		// Otherwise consolidate the measurements and move on:
		onewire_depower();
		sensors_consolidate_samples(wakeup);

		// If this is wakeup round 0, 1 or 2, then store the data to
		// RTC memory and go to sleep:
		if (wakeup < SENSORS_RECORDS_MAX - 1) {
			state_change(STATE_SENSORS_SAVE);
			return true;
		}

		// Otherwise, send the data:
		state_change(STATE_SENSORS_SEND);
		return true;

	// Save measurements to RTC memory and go to sleep:
	case STATE_SENSORS_SAVE:
		os_printf("Saving measurements to RTC memory: %s\n",
			rtc_mem_save(wakeup + 1) ? "success" : "fail");
		deep_sleep();
		return true;

	// Consolidate measurements and send over wifi:
	case STATE_SENSORS_SEND:
		os_printf("Sending measurements to wifi\n");

		// Reset RTC memory to zero records:
		rtc_mem_save(0);

		// Consolidate records and send to wifi:
		sensors_consolidate_records();
		state_change(STATE_WIFI_SETUP_START);
		return true;

	default:
		return false;
	}
}

// Wifi event handler
static bool ICACHE_FLASH_ATTR
wifi_event (os_event_t *event)
{
	static uint8_t round = 0;

	switch (event->sig)
	{
	// Start setting up wifi:
	case STATE_WIFI_SETUP_START:
		led_blink(100);
		if (!wifi_connect())
			state_change(STATE_WIFI_SETUP_FAIL);
		return true;

	// Wifi setup or connect failed:
	case STATE_WIFI_SETUP_FAIL:
		led_blink(500);
		os_printf("Wifi setup failed!\n");
		if (round++ < 3) {
			os_printf("Wifi: retrying (%u)\n", round);
			state_change(STATE_WIFI_SETUP_START);
		}
		else if (!wifi_shutdown())
			state_change(STATE_WIFI_SHUTDOWN_DONE);
		return true;

	// Wifi is successfully setup:
	case STATE_WIFI_SETUP_DONE:
		os_printf("Wifi setup done\n");
		state_change(STATE_NET_CONNECT_START);
		return true;

	// Start shutting down wifi:
	case STATE_WIFI_SHUTDOWN_START:
		os_printf("Wifi shutdown starting\n");
		if (!wifi_shutdown())
			state_change(STATE_WIFI_SHUTDOWN_DONE);
		return true;

	// Wifi has been successfully shut down:
	case STATE_WIFI_SHUTDOWN_DONE:
		os_printf("Wifi shutdown done\n");
		deep_sleep();
		return true;

	default:
		return false;
	}
}

// Network event handler
static bool ICACHE_FLASH_ATTR
net_event (os_event_t *event)
{
	switch (event->sig)
	{
	// Start setting up network connection:
	case STATE_NET_CONNECT_START:
		led_blink(200);
		if (!net_connect())
			state_change(STATE_NET_CONNECT_FAIL);
		return true;

	// Network connection setup failed:
	case STATE_NET_CONNECT_FAIL:
		os_printf("Network connect failed!\n");
		if (!net_disconnect())
			state_change(STATE_NET_DISCONNECT_DONE);
		return true;

	// Network connection successfully setup:
	case STATE_NET_CONNECT_DONE: {
		size_t len;
		char *buf = http_post_create(&len);

		os_printf("Network connect done\n");
		if (!net_send(buf, len))
			state_change(STATE_NET_DATA_SENT);
		return true;
	}

	// Network data sent:
	case STATE_NET_DATA_SENT:
		os_printf("Network data sent\n");
		http_post_destroy();
		net_disconnect();
		return true;

	// Network successfully disconnected:
	case STATE_NET_DISCONNECT_DONE:
		os_printf("Network disconnect done\n");
		state_change(STATE_WIFI_SHUTDOWN_START);
		return true;

	default:
		return false;
	}
}

// System event handler
static void ICACHE_FLASH_ATTR
on_event (os_event_t *event)
{
	if (sensor_event(event))
		return;

	if (wifi_event(event))
		return;

	if (net_event(event))
		return;

	os_printf("Unhandled event: 0x%08x, payload 0x%08x\n", event->sig, event->par);
}

// Entry point after system init
static void ICACHE_FLASH_ATTR
on_init_done (void)
{
	char *flash_map[] = {
		[FLASH_SIZE_2M]			= "2 MB",
		[FLASH_SIZE_4M_MAP_256_256]	= "4 MB, 256x256",
		[FLASH_SIZE_8M_MAP_512_512]	= "8 MB, 512x512",
		[FLASH_SIZE_16M_MAP_512_512]	= "16 MB, 512x512",
		[FLASH_SIZE_16M_MAP_1024_1024]	= "16 MB, 1024x1024",
		[FLASH_SIZE_32M_MAP_512_512]	= "32 MB, 512x512",
		[FLASH_SIZE_32M_MAP_1024_1024]	= "32 MB, 1024x1024",
	};

	char *reset_map[] = {
		[REASON_DEFAULT_RST]		= "normal power on",
		[REASON_WDT_RST]		= "hardware watchdog reset",
		[REASON_EXCEPTION_RST]		= "exception reset",
		[REASON_SOFT_WDT_RST]		= "soft restart by watchdog",
		[REASON_SOFT_RESTART]		= "soft restart",
		[REASON_DEEP_SLEEP_AWAKE]	= "deep sleep awakening",
		[REASON_EXT_SYS_RST]		= "external system reset",
	};

	struct rst_info *reset_info = system_get_rst_info();

	os_printf("SDK version    : %s\n", system_get_sdk_version());
	os_printf("Chip ID        : 0x%08x\n", system_get_chip_id());
	os_printf("VDD 3.3        : %u mV\n", (readvdd33() * 1000) / 1024);
	os_printf("Boot version   : %u\n", system_get_boot_version());
	os_printf("CPU frequency  : %u MHz\n", system_get_cpu_freq());
	os_printf("Flash size map : %s\n", flash_map[system_get_flash_size_map()]);
	os_printf("Reset info     : %s\n", reset_map[reset_info->reason]);

	system_print_meminfo();

	// If we were reset by awaking from deep sleep, we read out RTC memory
	// to import information from earlier rounds. Find out which wakeup
	// round this is:
	if (reset_info->reason == REASON_DEEP_SLEEP_AWAKE) {
		wakeup = rtc_mem_load();
		os_printf("Wakeup %u\n", wakeup);
	}

	// Start by getting sensor measurements:
	state_change(STATE_SENSORS_START);
}

// Entry point at system init
void ICACHE_FLASH_ATTR
user_init (void)
{
	static os_event_t events[NUM_EVENTS];

	gpio_init();
	uart_init();
	onewire_init();

	// Call this function after system init is complete:
	system_init_done_cb(on_init_done);

	// Event handler:
	system_os_task(on_event, 0, events, NUM_EVENTS);
}
