#include <osapi.h>
#include <user_interface.h>

#include "missing.h"
#include "secrets.h"
#include "state.h"

// Remember connection events:
static bool is_connected = false;

// Wifi event handler for connect
static void ICACHE_FLASH_ATTR
on_connect_event (System_Event_t *event)
{
	switch (event->event)
	{
	case EVENT_STAMODE_CONNECTED:
		os_printf("Wifi: connected\n");
		os_printf("RSSI: %d\n", wifi_station_get_rssi());
		is_connected = true;
		break;

	case EVENT_STAMODE_DISCONNECTED:
		switch (wifi_station_get_connect_status())
		{
		case STATION_WRONG_PASSWORD:
			os_printf("Wifi connect: disconnected, wrong password\n");
			break;

		case STATION_NO_AP_FOUND:
			os_printf("Wifi connect: disconnected, AP \"" SECRET_SSID "\" not found\n");
			break;

		default:
			os_printf("Wifi connect: disconnected\n");
			break;
		}
		is_connected = false;
		state_change(STATE_WIFI_SETUP_FAIL);
		break;

	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("Wifi connect: auth mode change\n");
		break;

	case EVENT_STAMODE_GOT_IP:
		os_printf("Wifi connect: got IP\n");
		state_change(STATE_WIFI_SETUP_DONE);
		break;

	case EVENT_STAMODE_DHCP_TIMEOUT:
		os_printf("Wifi connect: DHCP timeout\n");
		state_change(STATE_WIFI_SETUP_FAIL);
		break;

	default:
		os_printf("Wifi connect: unhandled event 0x%x\n", event->event);
		break;
	}
}

// Set wifi to station mode
static bool ICACHE_FLASH_ATTR
set_opmode (const uint8_t opmode)
{
	if (wifi_set_opmode_current(opmode))
		return true;

	os_printf("Wifi: couldn't set opmode 0x%x!\n", opmode);
	return false;
}

// Configure wifi SSID and password
static bool ICACHE_FLASH_ATTR
configure (void)
{
	// Don't check AP's MAC address:
	struct station_config config = { .bssid_set = 0 };

	os_memcpy(&config.ssid,     SECRET_SSID,     sizeof(SECRET_SSID));
	os_memcpy(&config.password, SECRET_PASSWORD, sizeof(SECRET_PASSWORD));

	if (wifi_station_set_config(&config))
		return true;

	os_printf("Wifi: couldn't set config!\n");
	return false;
}

// Establish wifi connection
static bool ICACHE_FLASH_ATTR
connect (void)
{
	if (wifi_station_connect())
		return true;

	os_printf("Wifi: couldn't connect!\n");
	return false;
}

// Setup a wifi connection
bool ICACHE_FLASH_ATTR
wifi_connect (void)
{
	if (!set_opmode(STATION_MODE))
		return false;

	if (!configure())
		return false;

	// Register event handler before connecting:
	wifi_set_event_handler_cb(on_connect_event);

	// Don't reconnect automatically:
	wifi_station_set_auto_connect(0);
	wifi_station_set_reconnect_policy(false);

	return connect();
}

// Wifi event handler for disconnect:
static void ICACHE_FLASH_ATTR
on_disconnect_event (System_Event_t *event)
{
	if (event->event != EVENT_STAMODE_DISCONNECTED) {
		os_printf("Wifi disconnect: unhandled event 0x%x\n", event->event);
		return;
	}

	os_printf("Wifi disconnect: disconnected\n");

	// Stop RF engine, hopefully:
	set_opmode(NULL_MODE);

	// Set sleep mode:
	if (!wifi_set_sleep_type(MODEM_SLEEP_T))
		os_printf("Wifi: couldn't set modem sleep mode!\n");

	// Signal state change:
	state_change(STATE_WIFI_SHUTDOWN_DONE);
}

// Tear down a wifi connection and sleep the modem
bool ICACHE_FLASH_ATTR
wifi_shutdown (void)
{
	// If the interface never connected during setup, we cannot disconnect
	// it. (The function will hang.) In that case, just call our callback
	// directly:
	if (!is_connected) {
		System_Event_t event = { .event = EVENT_STAMODE_DISCONNECTED };
		on_disconnect_event(&event);
		return true;
	}

	// Register a callback for when we've disconnected:
	wifi_set_event_handler_cb(on_disconnect_event);

	if (wifi_station_disconnect())
		return true;

	os_printf("Wifi: couldn't disconnect!\n");
	return false;
}
