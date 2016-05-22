#define SENSORS_ROUNDS_MAX	3

void sensors_request (const size_t round);
void sensors_readout (const size_t round);
void sensors_consolidate_samples (void);
bool sensors_all_valid (void);
