// BPW34 photodiode + TIA test sketch for Teensy 4.0
// Reads analog voltage from the transimpedance amplifier output
// and prints to Serial Monitor every second.
// Wiring: TIA output -> pin 15 (A01 on MTB)

#include <SPI.h>
#include <SD.h>

#define BPW34_PIN A1  // Teensy pin 15 = A01 on MTB

#define ADC_BITS 12
#define ADC_MAX  ((1 << ADC_BITS) - 1)  // 4095 for 12-bit
#define VREF     3.3f

#define PRINT_INTERVAL_MS 1000

static unsigned long lastPrintTime = 0;
static bool sdReady = false;
static char logFilename[20];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  // Disable motor pins -- they float on boot and the H-bridge will run them.
  const uint8_t motorPins[] = {3, 4, 5, 6, 22, 23};
  for (uint8_t i = 0; i < sizeof(motorPins); i++) {
    pinMode(motorPins[i], OUTPUT);
    digitalWrite(motorPins[i], LOW);
  }

  analogReadResolution(ADC_BITS);
  pinMode(BPW34_PIN, INPUT);

  // Initialize SD card
  Serial.print("Initializing SD Card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "BPW34%03d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,ADC_Raw,Voltage");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  Serial.println("=== BPW34 Photodiode Test ===");
  Serial.print("Pin: A1 (Teensy pin 15), ");
  Serial.print(ADC_BITS);
  Serial.println("-bit ADC");
  Serial.println("Time,ADC_Raw,Voltage");
  Serial.println();

  lastPrintTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    int raw = analogRead(BPW34_PIN);
    float voltage = (float)raw * VREF / ADC_MAX;

    Serial.print(now);
    Serial.print(",");
    Serial.print(raw);
    Serial.print(",");
    Serial.println(voltage, 4);

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(raw);
        file.print(",");
        file.println(voltage, 4);
        file.close();
      }
    }

    lastPrintTime = now;
  }
}
