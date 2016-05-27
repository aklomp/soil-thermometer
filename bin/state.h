enum state {
	STATE_SENSORS_START,
	STATE_SENSORS_READOUT,
	STATE_SENSORS_DONE,
	STATE_SENSORS_SAVE,
	STATE_SENSORS_SEND,
	STATE_WIFI_SETUP_START,
	STATE_WIFI_SETUP_FAIL,
	STATE_WIFI_SETUP_DONE,
	STATE_WIFI_SHUTDOWN_START,
	STATE_WIFI_SHUTDOWN_DONE,
	STATE_NET_CONNECT_START,
	STATE_NET_CONNECT_FAIL,
	STATE_NET_CONNECT_DONE,
	STATE_NET_DATA_SENT,
	STATE_NET_DISCONNECT_DONE,
};

void state_change (enum state);
