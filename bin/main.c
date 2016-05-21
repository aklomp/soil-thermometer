#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>

#include "http.h"
#include "led.h"
#include "missing.h"
#include "net.h"
#include "state.h"
#include "uart.h"
#include "wifi.h"

#define NUM_EVENTS	4
#define DEEP_SLEEP_SEC	900UL
#define DEEP_SLEEP_USEC	(DEEP_SLEEP_SEC * 1000000UL)

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
	case STATE_NET_CONNECT_DONE:
		os_printf("Network connect done\n");
		if (!net_send(http_post_create()))
			state_change(STATE_NET_DATA_SENT);
		return true;

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

	// Start setting up wifi:
	state_change(STATE_WIFI_SETUP_START);
}

// Entry point at system init
void ICACHE_FLASH_ATTR
user_init (void)
{
	static os_event_t events[NUM_EVENTS];

	gpio_init();
	uart_init();

	// Call this function after system init is complete:
	system_init_done_cb(on_init_done);

	// Event handler:
	system_os_task(on_event, 0, events, NUM_EVENTS);
}
