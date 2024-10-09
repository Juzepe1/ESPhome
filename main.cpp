#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>

#define RE 2
#define DE 1

// Modbus RTU requests for individual registers (without CRC)
const byte requests[][6] = {
  {0x01, 0x03, 0x00, 0x00, 0x00, 0x01},  // Register 0 - Humidity
  {0x01, 0x03, 0x00, 0x01, 0x00, 0x01},  // Register 1 - Temperature
  {0x01, 0x03, 0x00, 0x02, 0x00, 0x01},  // Register 2 - Conductivity
  {0x01, 0x03, 0x00, 0x03, 0x00, 0x01},  // Register 3 - pH
  {0x01, 0x03, 0x00, 0x04, 0x00, 0x01},  // Register 4 - Nitrogen (N)
  {0x01, 0x03, 0x00, 0x05, 0x00, 0x01},  // Register 5 - Phosphorus (P)
  {0x01, 0x03, 0x00, 0x06, 0x00, 0x01},  // Register 6 - Potassium (K)
  {0x01, 0x03, 0x00, 0x07, 0x00, 0x01}   // Register 7 - Salinity
};



  // Array to store the response (for each register read)
SoftwareSerial mod(3, 0);

uint16_t calculateCRC(const byte* data, byte length);
byte readRegister(const byte* request, byte length);

void setup() {
  Serial.begin(115200);
  mod.begin(4800);
  
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  
  delay(500);
}

void loop() {
  // Iterate through registers 0 to 7 and read them one by one
  for (byte i = 0; i < 8; i++) {
    byte value = readRegister(requests[i], sizeof(requests[i]));

    if (value != -1) {  // Check if the reading was valid
      // Print each value with a label based on the register index
      switch (i) {
        case 0:
          Serial.print("Humidity: ");
          Serial.print(value / 10.0);  // Convert to %RH
          Serial.println(" %");
          break;
        case 1:
          Serial.print("Temperature: ");
          Serial.print(value / 10.0);  // Convert to °C
          Serial.println(" °C");
          break;
        case 2:
          Serial.print("Conductivity: ");
          Serial.print(value);  // µS/cm
          Serial.println(" µS/cm");
          break;
        case 3:
          Serial.print("pH: ");
          Serial.print(value / 10.0);  // pH scale
          Serial.println();
          break;
        case 4:
          Serial.print("Nitrogen (N): ");
          Serial.print(value);  // mg/kg
          Serial.println(" mg/kg");
          break;
        case 5:
          Serial.print("Phosphorus (P): ");
          Serial.print(value);  // mg/kg
          Serial.println(" mg/kg");
          break;
        case 6:
          Serial.print("Potassium (K): ");
          Serial.print(value);  // mg/kg
          Serial.println(" mg/kg");
          break;
        case 7:
          Serial.print("Salinity: ");
          Serial.print(value);  // mg/L
          Serial.println(" mg/L");
          break;
      }
    } else {
      Serial.print("Failed to read register ");
      Serial.println(i);
    }

    delay(100);  // Delay between each register read
  }

  delay(1000);  // Optional: Add a longer delay before looping back
}

// Function to send Modbus request, read sensor data, and calculate CRC
byte readRegister(const byte* request, byte length) {
  
  digitalWrite(DE, HIGH);
  digitalWrite(RE, HIGH);
  delay(10);
  
  // Send the request (without CRC)
  mod.write(request, length);
  
  // Calculate and append the CRC to the request
  uint16_t crc = calculateCRC(request, length);  
  mod.write(crc & 0xFF);       // Send CRC Low byte
  mod.write(crc >> 8);         // Send CRC High byte
  
  digitalWrite(DE, LOW);
  digitalWrite(RE, LOW);
  
  delay(10);  // Wait for response

  unsigned long startTime = millis();
  while(mod.available() < 7 && millis() - startTime < 1000) {
    delay(1);
  }
  if(mod.available() < 7) {
    return -1;
  }

  byte values[7];
  // Check if there is data available to read
  byte i = 0;
  while(mod.available() && i < 7) {
    values[i++] = mod.read();
  }

  // Calculate CRC on the received response (first 5 bytes)
  uint16_t received_crc = (values[6] << 8) | values[5];  // Received CRC (from values[5] and values[6])
  uint16_t calculated_crc = calculateCRC(values, 5);     // Calculated CRC based on response data

  // Check for CRC mismatch
  if (received_crc != calculated_crc) {
    Serial.println("Error: CRC mismatch! Invalid reading.");
    return -1;  // Return an invalid value
  }

  // Extract the sensor value (from byte 3 and 4)
  return (values[3] << 8) | values[4];
}

// Function to calculate CRC-16 (Modbus)
uint16_t calculateCRC(const byte* data, byte length) {
  uint16_t crc = 0xFFFF;  // Initialize the CRC value

  for (byte i = 0; i < length; i++) {
    crc ^= data[i];  // XOR byte into least significant byte of crc

    for (byte j = 0; j < 8; j++) {  // Loop over each bit
      if (crc & 0x0001) {
        crc >>= 1;       // Shift right and XOR with polynomial
        crc ^= 0xA001;   // Modbus polynomial
      } else {
        crc >>= 1;       // Just shift right
      }
    }
  }
  
  return crc;  // Return the 16-bit CRC
}
