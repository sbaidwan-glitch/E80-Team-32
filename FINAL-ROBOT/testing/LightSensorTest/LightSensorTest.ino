// Combined VEML6030 + BPW34 light sensor test for Teensy 4.0
// Reads both sensors simultaneously and logs to serial + SD card.
//
// VEML6030: ambient light via I2C (SDA=18, SCL=19), address 0x48
// BPW34:   photodiode TIA analog output on pin 15 (A1)

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// ---- VEML6030 config ----
#define VEML6030_ADDR      0x48
#define VEML6030_REG_CONF  0x00
#define VEML6030_REG_ALS   0x04
#define VEML6030_REG_WHITE 0x05
#define ALS_CONF_VALUE     0x1300   // Gain x1/8, IT 25ms
#define LUX_PER_COUNT      1.8432f  // resolution at this gain/IT

// ---- BPW34 config ----
#define BPW34_PIN   A1              // Teensy pin 15
#define ADC_BITS    12
#define ADC_MAX     ((1 << ADC_BITS) - 1)  // 4095
#define VREF        3.3f

// ---- Timing ----
#define PRINT_INTERVAL_MS  1000

static unsigned long lastPrintTime = 0;
static bool sdReady = false;
static char logFilename[20];

// ---- VEML6030 I2C helpers ----
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

  // ---- I2C + VEML6030 init ----
  Wire.begin();

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
    Serial.println("  NO devices found! Check wiring.");
  }
  Serial.println();

  writeRegister(VEML6030_REG_CONF, ALS_CONF_VALUE);
  delay(5);

  uint16_t check = readRegister(VEML6030_REG_CONF);
  Serial.print("VEML6030: ");
  if (check == ALS_CONF_VALUE) {
    Serial.println("Init OK");
  } else {
    Serial.print("Init FAIL (read 0x");
    Serial.print(check, HEX);
    Serial.println(")");
  }

  // ---- BPW34 init ----
  analogReadResolution(ADC_BITS);
  pinMode(BPW34_PIN, INPUT);
  Serial.println("BPW34:    Init OK (pin A1, 12-bit ADC)");
  Serial.println();

  // ---- SD card init ----
  Serial.print("Initializing SD card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "LIGHT%03d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,ALS_Raw,Lux,White_Raw,BPW34_ADC,BPW34_V");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  Serial.println();
  Serial.println("Time_ms,ALS_Raw,Lux,White_Raw,BPW34_ADC,BPW34_V");

  lastPrintTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    // Read VEML6030
    uint16_t alsRaw = readRegister(VEML6030_REG_ALS);
    float lux = alsRaw * LUX_PER_COUNT;
    uint16_t whiteRaw = readRegister(VEML6030_REG_WHITE);

    // Read BPW34
    int bpwRaw = analogRead(BPW34_PIN);
    float bpwVoltage = (float)bpwRaw * VREF / ADC_MAX;

    // Print CSV
    Serial.print(now);
    Serial.print(",");
    Serial.print(alsRaw);
    Serial.print(",");
    Serial.print(lux, 1);
    Serial.print(",");
    Serial.print(whiteRaw);
    Serial.print(",");
    Serial.print(bpwRaw);
    Serial.print(",");
    Serial.println(bpwVoltage, 4);

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
        file.print(whiteRaw);
        file.print(",");
        file.print(bpwRaw);
        file.print(",");
        file.println(bpwVoltage, 4);
        file.close();
      }
    }

    lastPrintTime = now;
  }
}
