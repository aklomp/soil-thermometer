#include <ets_sys.h>
#include <osapi.h>
#include <user_interface.h>

#include "uart.h"

// Function prototypes missing in SDK:
extern int os_printf_plus (const char *format, ...) __attribute__ ((format (printf, 1, 2)));

// Entry point
void ICACHE_FLASH_ATTR
user_init (void)
{
	uart_init();
	os_printf("SDK version: %s\n", system_get_sdk_version());
}
