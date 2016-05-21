#include <ets_sys.h>

#include "http.h"

// Create a HTTP POST message
char * ICACHE_FLASH_ATTR
http_post_create (void)
{
	return	"POST / HTTP/1.0\r\n"
		"Host: esp8266-ds18b20\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: 12\r\n"
		"\r\n"
		"Hello world\n";
}

// Destroy the HTTP POST message
void ICACHE_FLASH_ATTR
http_post_destroy (void)
{
}
