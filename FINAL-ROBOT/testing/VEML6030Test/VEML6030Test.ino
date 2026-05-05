// VEML6030 ambient light sensor test sketch for Teensy 4.0
// Reads lux over I2C and prints to Serial Monitor every second.
// Wiring: SDA = pin 18, SCL = pin 19, ADDR = GND
// Pull-ups: 4.7k to 3.3V on SDA and SCL

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define VEML6030_ADDR      0x48
#define VEML6030_REG_CONF  0x00
#define VEML6030_REG_ALS   0x04
#define VEML6030_REG_WHITE 0x05

// Gain x1/8, IT 25ms -> resolution 1.8432 lux/count, max ~120k lux
#define ALS_CONF_VALUE     0x1300
#define LUX_PER_COUNT      1.8432f

#define PRINT_INTERVAL_MS  1000

static unsigned long lastPrintTime = 0;
static bool sdReady = false;
static char logFilename[20];

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

  // Disable motor pins -- they float on boot and the H-bridge will run them.
  const uint8_t motorPins[] = {3, 4, 5, 6, 22, 23};
  for (uint8_t i = 0; i < sizeof(motorPins); i++) {
    pinMode(motorPins[i], OUTPUT);
    digitalWrite(motorPins[i], LOW);
  }

  Wire.begin();

  // --- I2C bus scan: find all devices ---
  Serial.println("=== I2C Bus Scan ===");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  NO devices found! Check wiring and pull-ups.");
  }
  Serial.println();

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
  // Initialize SD card
  Serial.print("Initializing SD Card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "VEML%04d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,ALS_Raw,Lux,White_Raw");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
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

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(alsRaw);
        file.print(",");
        file.print(lux, 1);
        file.print(",");
        file.println(whiteRaw);
        file.close();
      }
    }

    lastPrintTime = now;
  }
}
