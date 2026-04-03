// VEML6030 ambient light sensor test sketch for Teensy 4.0
// Reads lux over I2C and prints to Serial Monitor every second.
// Wiring: SDA = pin 18, SCL = pin 19, ADDR = GND
// Pull-ups: 4.7k to 3.3V on SDA and SCL

#include <Wire.h>

#define VEML6030_ADDR      0x10
#define VEML6030_REG_CONF  0x00
#define VEML6030_REG_ALS   0x04
#define VEML6030_REG_WHITE 0x05

// Gain x1/8, IT 25ms -> resolution 1.8432 lux/count, max ~120k lux
#define ALS_CONF_VALUE     0x1300
#define LUX_PER_COUNT      1.8432f

#define PRINT_INTERVAL_MS  1000

static unsigned long lastPrintTime = 0;

void writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.write(value & 0xFF);
  Wire.write((value >> 8) & 0xFF);
  Wire.endTransmission();
}

uint16_t readRegister(uint8_t reg) {
  Wire.beginTransmission(VEML6030_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)VEML6030_ADDR, (uint8_t)2);
  uint16_t val = Wire.read();
  val |= ((uint16_t)Wire.read() << 8);
  return val;
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  Wire.begin();
  writeRegister(VEML6030_REG_CONF, ALS_CONF_VALUE);
  delay(5);

  uint16_t check = readRegister(VEML6030_REG_CONF);
  Serial.println("=== VEML6030 Test ===");
  if (check == ALS_CONF_VALUE) {
    Serial.println("Init OK");
  } else {
    Serial.print("Init FAIL (read 0x");
    Serial.print(check, HEX);
    Serial.println("). Check wiring: SDA=18, SCL=19, ADDR=GND");
  }
  Serial.println("Time,ALS_Raw,Lux,White_Raw");
  Serial.println();

  lastPrintTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    uint16_t alsRaw = readRegister(VEML6030_REG_ALS);
    float lux = alsRaw * LUX_PER_COUNT;
    uint16_t whiteRaw = readRegister(VEML6030_REG_WHITE);

    Serial.print(now);
    Serial.print(",");
    Serial.print(alsRaw);
    Serial.print(",");
    Serial.print(lux, 1);
    Serial.print(",");
    Serial.println(whiteRaw);

    lastPrintTime = now;
  }
}
