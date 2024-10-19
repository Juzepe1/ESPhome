#ifndef WRITE_TO_MODBUS_DEVICE_H
#define WRITE_TO_MODBUS_DEVICE_H

#include <vector>
#include "esphome/components/modbus_controller/modbus_controller.h"  // include the necessary headers

void write_to_modbus_device(esphome::modbus_controller::ModbusController *modbus_device, uint16_t reg_address, uint16_t value);

#endif // WRITE_TO_MODBUS_DEVICE_H