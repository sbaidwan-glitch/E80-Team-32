#include "SensorVEML6030.h"
#include "Printer.h"
extern Printer printer;

SensorVEML6030::SensorVEML6030(void)
  : DataSource("als_raw,lux", "uint16,float")
{}

void SensorVEML6030::init(void) {
  Wire.begin();

  // ALS_CONF (0x00): gain x1/8, IT 25ms, power on  -> 0x1300
  writeRegister(VEML6030_REG_CONF, 0x1300);
  delay(5);

  uint16_t check = readRegister(VEML6030_REG_CONF);
  if (check == 0x1300) {
    Serial.println("VEML6030 init OK");
  } else {
    Serial.print("VEML6030 init FAIL (read 0x");
    Serial.print(check, HEX);
    Serial.println("). Check SDA=18, SCL=19, ADDR=VDD for 0x48");
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
  Wire.write(value & 0xFF);
  Wire.write((value >> 8) & 0xFF);
  Wire.endTransmission();
}

uint16_t SensorVEML6030::readRegister(uint8_t reg) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)VEML6030_ADDR, (uint8_t)2);
  uint16_t val = Wire.read();
  val |= ((uint16_t)Wire.read() << 8);
  return val;
}
