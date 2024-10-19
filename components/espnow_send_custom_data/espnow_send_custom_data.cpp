#include "espnow_send_custom_data.h"

std::vector<unsigned char> format_espnow_data(const char* device_id, const char* sensor_id, float sensor_value) {
    char buffer[200];
    snprintf(buffer, sizeof(buffer), "%s;%s;%.0f", device_id, sensor_id, sensor_value);
    return std::vector<unsigned char>(buffer, buffer + strlen(buffer));
}