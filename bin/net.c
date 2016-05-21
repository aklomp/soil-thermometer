#include <osapi.h>
#include <user_interface.h>
#include <ip_addr.h>
#include <espconn.h>

#include "missing.h"
#include "net.h"
#include "state.h"

// Remember connection events:
static bool is_connected = false;

// Interpret error codes
static bool ICACHE_FLASH_ATTR
check_error (const char *func, int8_t error)
{
	const char *msg;

	switch (error)
	{
	case ESPCONN_OK:		return true;
	case ESPCONN_MEM:		msg = "out of memory";		break;
	case ESPCONN_TIMEOUT:		msg = "timeout";		break;
	case ESPCONN_RTE:		msg = "routing error";		break;
	case ESPCONN_INPROGRESS:	msg = "operation in progress";	break;
	case ESPCONN_MAXNUM:		msg = "total number > max";	break;
	case ESPCONN_ABRT:		msg = "connection aborted";	break;
	case ESPCONN_RST:		msg = "connection reset";	break;
	case ESPCONN_CLSD:		msg = "connection closed";	break;
	case ESPCONN_CONN:		msg = "not connected";		break;
	case ESPCONN_ARG:		msg = "config invalid";		break;
	case ESPCONN_IF:		msg = "UDP send error";		break;
	case ESPCONN_ISCONN:		msg = "already connected";	break;
	case ESPCONN_HANDSHAKE:		msg = "SSL handshake failed";	break;
	case ESPCONN_SSL_INVALID_DATA:	msg = "SSL application error";	break;
	default:			msg = "unknown error";		break;
	}

	os_printf("%s: %s\n", func, msg);
	return false;
}

// Connect callback
static void ICACHE_FLASH_ATTR
on_connect (void *data)
{
	os_printf("Net: connected\n");
	is_connected = true;
	state_change(STATE_NET_CONNECT_DONE);
}

// Reconnect callback
static void ICACHE_FLASH_ATTR
on_reconnect (void *data, int8_t error)
{
	// Print error:
	check_error("Net reconnected", error);

	// Consider connection failed:
	state_change(STATE_NET_CONNECT_FAIL);
}

// Write finished callback
static void ICACHE_FLASH_ATTR
on_write_finish (void *data)
{
	os_printf("Net: write finished\n");
	state_change(STATE_NET_DATA_SENT);
}

// Disconnect callback
static void ICACHE_FLASH_ATTR
on_disconnect (void *data)
{
	os_printf("Net: disconnected\n");
	is_connected = false;
	check_error("espconn_delete()", espconn_delete(data));
	state_change(STATE_NET_DISCONNECT_DONE);
}

// Connect
static bool ICACHE_FLASH_ATTR
connect (struct espconn *conn)
{
	os_printf("Net: connecting to %u.%u.%u.%u:%u\n",
		conn->proto.tcp->remote_ip[0],
		conn->proto.tcp->remote_ip[1],
		conn->proto.tcp->remote_ip[2],
		conn->proto.tcp->remote_ip[3],
		conn->proto.tcp->remote_port);

	return check_error("espconn_connect()", espconn_connect(conn));
}

// Get local IP address
bool ICACHE_FLASH_ATTR
get_local_ip (uint8_t *dest)
{
	struct ip_info info;

	if (!wifi_get_ip_info(0, &info)) {
		os_printf("Net init: could not get local IP!\n");
		return false;
	}

	os_memcpy(dest, &info.ip.addr, sizeof(info.ip.addr));
	return true;
}

static esp_tcp tcp = {
	.remote_port		= REMOTE_PORT,
	.remote_ip		= REMOTE_IP,
	.connect_callback	= on_connect,
	.reconnect_callback	= on_reconnect,
	.disconnect_callback	= on_disconnect,
	.write_finish_fn	= on_write_finish,
};

static struct espconn conn = {
	.type			= ESPCONN_TCP,
	.state			= ESPCONN_NONE,
	.proto.tcp		= &tcp,
	.recv_callback		= NULL,
	.sent_callback		= NULL,
};

// Initialize TCP connection to server
bool ICACHE_FLASH_ATTR
net_connect (void)
{
	// Fetch local IP address:
	if (!get_local_ip(tcp.local_ip))
		return false;

	// Fetch local port:
	tcp.local_port = espconn_port();

	// Do the actual connecting:
	return connect(&conn);
}

// Close TCP connection to server
bool ICACHE_FLASH_ATTR
net_disconnect (void)
{
	// If we're not connected, then calling espconn_disconnect() will hang.
	// In that case, call the callback directly:
	if (!is_connected) {
		on_disconnect(&conn);
		return true;
	}

	return check_error("espconn_disconnect()", espconn_disconnect(&conn));
}

// Send data after connecting
bool ICACHE_FLASH_ATTR
net_send (uint8_t *buf, size_t len)
{
	if (!buf || !len)
		return false;

	os_printf(buf);
	return check_error("espconn_send()", espconn_send(&conn, buf, len));
}
