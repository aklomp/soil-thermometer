#define REMOTE_PORT	80
#define REMOTE_IP	{ 192, 168, 178, 13 }

bool net_connect (void);
bool net_disconnect (void);
bool net_send (uint8_t *buf, size_t len);
