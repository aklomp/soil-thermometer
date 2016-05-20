#include <ets_sys.h>
#include <os_type.h>
#include <osapi.h>
#include <gpio.h>

// Function prototypes missing in SDK:
extern void ets_timer_arm_new (os_timer_t *, uint32_t, uint32_t, uint32_t);
extern void ets_timer_disarm (os_timer_t *);
extern void ets_timer_setfn (os_timer_t *, ETSTimerFunc *, void *);

// The onboard LED is at pin 2:
#define PIN_LED 2

static os_timer_t timer;

static inline void
led_off (void)
{
	GPIO_OUTPUT_SET(PIN_LED, 0);
}

static inline void
led_on (void)
{
	GPIO_OUTPUT_SET(PIN_LED, 1);
}

static void ICACHE_FLASH_ATTR
led_toggle (void)
{
	static bool state;

	(state) ? led_off() : led_on();
	state = !state;
}

// Timer tick callback
static void ICACHE_FLASH_ATTR
on_tick (void *data)
{
	led_toggle();
}

// Start blinking the onboard LED
void
led_blink (const uint32_t period_ms)
{
	// Enable pin 2 as output pin:
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	// Start in off position:
	os_timer_disarm(&timer);
	led_off();

	// LED can be disabled by setting period to 0:
	if (!period_ms)
		return;

	// Start timer:
	os_timer_setfn(&timer, (os_timer_func_t *) on_tick, NULL);
	os_timer_arm(&timer, period_ms, 1);
}
