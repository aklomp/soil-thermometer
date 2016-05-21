#include <osapi.h>
#include <user_interface.h>

#include "missing.h"
#include "secrets.h"

// Wifi event handler for connect
static void ICACHE_FLASH_ATTR
on_connect_event (System_Event_t *event)
{
	switch (event->event)
	{
	case EVENT_STAMODE_CONNECTED:
		os_printf("Wifi: connected\n");
		os_printf("RSSI: %d\n", wifi_station_get_rssi());
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
		break;

	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("Wifi connect: auth mode change\n");
		break;

	case EVENT_STAMODE_GOT_IP:
		os_printf("Wifi connect: got IP\n");
		break;

	case EVENT_STAMODE_DHCP_TIMEOUT:
		os_printf("Wifi connect: DHCP timeout\n");
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
	wifi_station_set_reconnect_policy(false);

	return connect();
}
