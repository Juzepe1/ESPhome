#ifndef ESPNOW_SEND_CUSTOM_DATA_H
#define ESPNOW_SEND_CUSTOM_DATA_H

#include <vector>
#include <string>
#include <cstring>

std::vector<unsigned char> format_espnow_data(const char* device_id, const char* sensor_id, float sensor_value);

#endif // ESPNOW_SEND_CUSTOM_DATA_H