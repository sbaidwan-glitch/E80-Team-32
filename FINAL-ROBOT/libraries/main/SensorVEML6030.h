#ifndef __SENSOR_VEML6030_H__
#define __SENSOR_VEML6030_H__

#include <Arduino.h>
#include <Wire.h>
#include "DataSource.h"

// I2C address when ADDR pin is tied to VDD (0x48) or GND (0x10)
#define VEML6030_ADDR       0x48

// Register addresses (datasheet Section 5, register map)
#define VEML6030_REG_CONF   0x00  // ALS configuration register
#define VEML6030_REG_ALS    0x04  // ALS data output register (16-bit)
#define VEML6030_REG_WHITE  0x05  // White channel data output

/*
 * SensorVEML6030 reads ambient light from the VEML6030 over I2C.
 * Teensy pins: 18 (SDA0), 19 (SCL0). Pull-ups: 4.7k to 3.3V.
 * Configured for outdoor sunlight (gain x1/8, integration time 25ms)
 * which gives a max range of ~120,000 lux.
 */
class SensorVEML6030 : public DataSource {
public:
  SensorVEML6030(void);

  void init(void);
  void read(void);

  String printState(void);
  size_t writeDataBytes(unsigned char * buffer, size_t idx);

  uint16_t alsRaw = 0;
  float lux = 0.0;

  int lastExecutionTime = -1;

private:
  void writeRegister(uint8_t reg, uint16_t value);
  uint16_t readRegister(uint8_t reg);

  static constexpr float RESOLUTION = 1.8432;
};

#endif
