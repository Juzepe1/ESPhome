#include "write_to_modbus_device.h"

void write_to_modbus_device(esphome::modbus_controller::ModbusController *modbus_device, uint16_t reg_address, uint16_t value) {
    uint16_t new_value = static_cast<uint16_t>(value);  // Cast the value to uint16_t
    std::vector<uint16_t> payload = {new_value};  // Create the payload with the new value

    // Create the Modbus write command
    esphome::modbus_controller::ModbusCommandItem command = 
        esphome::modbus_controller::ModbusCommandItem::create_write_multiple_command(modbus_device, reg_address, payload.size(), payload);

    // Queue the command to be sent
    modbus_device->queue_command(command);
}