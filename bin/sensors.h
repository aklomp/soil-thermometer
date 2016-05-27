#define SENSORS_RECORDS_MAX	4
#define SENSORS_ROUNDS_MAX	3

void sensors_request (const size_t round);
void sensors_readout (const size_t round);
void sensors_consolidate_samples (const size_t record);
void sensors_consolidate_records (void);
bool sensors_all_valid (void);
size_t sensors_json (char *buf);
uint8_t sensors_record_size (void);
void *sensors_record_data (const uint8_t n);
