#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>
#include <user_interface.h>
#include <gpio.h>

#include "led.h"
#include "uart.h"

// Function prototypes missing in SDK:
extern int os_printf_plus (const char *format, ...) __attribute__ ((format (printf, 1, 2)));

#define NUM_EVENTS	4

// System event handler
static void ICACHE_FLASH_ATTR
on_event (os_event_t *event)
{
	os_printf("Got event: 0x%x, payload 0x%x\n", event->sig, event->par);
}

// Entry point after system init
static void ICACHE_FLASH_ATTR
on_init_done (void)
{
	os_printf("Init complete\n");
	os_printf("SDK version: %s\n", system_get_sdk_version());

	led_blink(150);
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
