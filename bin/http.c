#include <ets_sys.h>
#include <os_type.h>
#include <mem.h>

#include "http.h"
#include "missing.h"

#define HEAD_SIZE	100
#define BODY_SIZE	1000
#define POST_SIZE	HEAD_SIZE + BODY_SIZE

// Static malloc'ed buffer:
static char *post = NULL;

// Allocate memory
static bool ICACHE_FLASH_ATTR
allocate (char **head, char **body, char **post)
{
	if ((*head = os_malloc(HEAD_SIZE)) == NULL)
		goto err0;

	if ((*body = os_malloc(BODY_SIZE)) == NULL)
		goto err1;

	if ((*post = os_malloc(POST_SIZE)) == NULL)
		goto err2;

	return true;

err2:	os_free(*body);
err1:	os_free(*head);
err0:	return false;
}

// Create header, return length
static size_t ICACHE_FLASH_ATTR
header_create (char *head, size_t body_len)
{
	return os_sprintf(head,
		"POST / HTTP/1.0\r\n"
		"Host: %s\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %u\r\n",
		REMOTE_SERVER,
		body_len
	);
}

// Create body, return length
static size_t ICACHE_FLASH_ATTR
body_create (char *body)
{
	return os_sprintf(body, "Hello, world!\n");
}

// Create a HTTP POST message
char * ICACHE_FLASH_ATTR
http_post_create (size_t *len)
{
	char *head = NULL;
	char *body = NULL;

	// Allocate memory:
	if (!allocate(&head, &body, &post))
		return NULL;

	// Create header and body:
	header_create(head, body_create(body));

	// Create HTTP message:
	*len = os_sprintf(post, "%s\r\n%s", head, body);

	// Don't need these any more:
	os_free(head);
	os_free(body);

	return post;
}

// Destroy the HTTP POST message
void ICACHE_FLASH_ATTR
http_post_destroy (void)
{
	os_free(post);
}
