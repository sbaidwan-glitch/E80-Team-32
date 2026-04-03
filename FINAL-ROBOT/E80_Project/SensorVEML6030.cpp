/*
 * HOW YOU'D FIGURE THIS OUT WITHOUT AI:
 *
 * The implementation follows two references:
 *
 * 1. The VEML6030 datasheet (felix-notes/resources/veml6030.pdf)
 *    - Section 5: register addresses and bit layouts for ALS_CONF
 *    - Section 6: I2C timing diagrams showing byte order (low byte first)
 *    - Table 1: resolution values for each gain/IT combo (1.8432 at 1/8, 25ms)
 *
 * 2. The E80 ButtonSampler.cpp (E80 Github/libraries/main/ButtonSampler.cpp)
 *    - Shows the pattern: constructor calls DataSource("colName","colType"),
 *      init() sets up hardware, writeDataBytes() copies data into the
 *      SD card buffer at the given index and returns the next free index.
 *    - The printState() function returns a String for the serial monitor.
 *
 * The Wire.beginTransmission / Wire.write / Wire.endTransmission pattern
 * is standard Arduino I2C — documented at arduino.cc Wire reference.
 * The "repeated start" (endTransmission(false)) before requestFrom is
 * required by most I2C sensors to keep the bus held between writing the
 * register address and reading the data back.
 */

#include "SensorVEML6030.h"
#include "Printer.h"
extern Printer printer;

SensorVEML6030::SensorVEML6030(void)
  : DataSource("als_raw,lux", "uint16,float") // logged columns
{}

void SensorVEML6030::init(void) {
  // Wire.begin() is safe to call multiple times (no-op if already started).
  // The IMU also uses Wire, so it's likely already running.
  Wire.begin();

  // ALS_CONF register (0x00) bit layout:
  //   [12:11] ALS_GAIN = 10  -> x1/8 gain (for bright outdoor light)
  //   [9:6]   ALS_IT   = 1100 -> 25ms integration time
  //   [0]     ALS_SD   = 0   -> power on
  // Full value: 0x1300
  writeRegister(VEML6030_REG_CONF, 0x1300);
  delay(5);  // let config settle

  // Verify the sensor is responding
  uint16_t check = readRegister(VEML6030_REG_CONF);
  if (check == 0x1300) {
    Serial.println("VEML6030 init OK");
  } else {
    Serial.print("VEML6030 init FAIL (read 0x");
    Serial.print(check, HEX);
    Serial.println("). Check wiring: SDA=pin18, SCL=pin19, ADDR=GND");
  }
}

void SensorVEML6030::read(void) {
  alsRaw = readRegister(VEML6030_REG_ALS);
  lux = alsRaw * RESOLUTION;
}

String SensorVEML6030::printState(void) {
  return "VEML: " + String(lux, 1) + " lux (raw=" + String(alsRaw) + ")";
}

size_t SensorVEML6030::writeDataBytes(unsigned char * buffer, size_t idx) {
  // Write als_raw (uint16) then lux (float) to log buffer
  uint16_t * slot16 = (uint16_t *) &buffer[idx];
  slot16[0] = alsRaw;
  idx += sizeof(uint16_t);

  float * slotF = (float *) &buffer[idx];
  slotF[0] = lux;
  idx += sizeof(float);

  return idx;
}

void SensorVEML6030::writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.write(value & 0xFF);         // low byte first (VEML6030 is little-endian)
  Wire.write((value >> 8) & 0xFF);  // high byte
  Wire.endTransmission();
}

uint16_t SensorVEML6030::readRegister(uint8_t reg) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom((uint8_t)VEML6030_ADDR, (uint8_t)2);
  uint16_t val = Wire.read();          // low byte
  val |= ((uint16_t)Wire.read() << 8); // high byte
  return val;
}
