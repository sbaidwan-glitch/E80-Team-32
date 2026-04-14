
#include <SPI.h>
#include <SD.h>

#define ANEMOMETER_PIN 16
#define PRINT_INTERVAL_MS 1000

static volatile unsigned long pulseCount = 0;

static void anemometerISR(void) {
  pulseCount++;
}

static unsigned long lastPrintTime = 0;
static bool sdReady = false;
static char logFilename[20];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // wait up to 3 s for Serial Monitor to connect
  }

  pinMode(ANEMOMETER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, RISING);

  // Initialize SD card
  Serial.print("Initializing SD Card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    // Find next available filename
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "ANEMO%03d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,Pulses,Freq_Hz");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  lastPrintTime = millis();
  pulseCount = 0;

  Serial.println("=== Anemometer Test ===");
  Serial.print("Pin: ");
  Serial.println(ANEMOMETER_PIN);
  Serial.println("Anemometer test ready. Spin the cups to see pulses.\n");
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastPrintTime;

  if (elapsed >= PRINT_INTERVAL_MS) {
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();

    float freq = (float)count / ((float)elapsed / 1000.0);

    Serial.print("Pulses: ");
    Serial.print(count);
    Serial.print("  |  Freq: ");
    Serial.print(freq, 2);
    Serial.println(" Hz");

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(count);
        file.print(",");
        file.println(freq, 2);
        file.close();
      }
    }

    lastPrintTime = now;
  }
}
