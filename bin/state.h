enum state {
	STATE_WIFI_SETUP_START,
	STATE_WIFI_SETUP_FAIL,
	STATE_WIFI_SETUP_DONE,
};

void state_change (enum state);
