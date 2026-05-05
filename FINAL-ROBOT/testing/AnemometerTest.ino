
#include <SPI.h>
#include <SD.h>

#define ANEMOMETER_PIN 14
#define PRINT_INTERVAL_MS 10000
#define MIN_PULSE_SPACING_US 10000  // debounce filter (microseconds)
#define N_AVG 5                     // moving average window for RPM

// Pulse counting
static volatile unsigned long pulseCount = 0;
static volatile unsigned long lastPulseMicros = 0;

// Timing
static unsigned long lastPrintTime = 0;

// SD logging
static bool sdReady = false;
static char logFilename[20];

// Moving average for RPM
float rpmHist[N_AVG] = {0};
int rpmIdx = 0;
bool rpmFilled = false;

// Calibration (fill AFTER wind tunnel)
float a_cal = 21.1f;
float b_cal = -1.71f;

// Interrupt: counts magnet passes with debounce
void anemometerISR() {
  unsigned long nowUs = micros();
  if (nowUs - lastPulseMicros >= MIN_PULSE_SPACING_US) {
    pulseCount++;
    lastPulseMicros = nowUs;
  }
}

// Moving average function
float movingAverageRPM(float newVal) {
  rpmHist[rpmIdx] = newVal;
  rpmIdx++;

  if (rpmIdx >= N_AVG) {
    rpmIdx = 0;
    rpmFilled = true;
  }

  int n = rpmFilled ? N_AVG : rpmIdx;
  if (n <= 0) return newVal;

  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    sum += rpmHist[i];
  }

  return sum / n;
}

// Convert RPM to velocity (after calibration)
float velocityFromRPM(float rpm) {
  return (rpm - b_cal) / a_cal;
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

  pinMode(ANEMOMETER_PIN, INPUT);
  // Sensor goes LOW when magnet passes -> use FALLING
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, FALLING);

  // Initialize SD card
  Serial.print("Initializing SD Card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "ANEMO%03d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("time_ms,pulses,rpm,velocity");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  delay(2000);

  Serial.println("START LOGGING");
  Serial.println("time_ms,pulses,rpm,velocity");

  lastPrintTime = millis();
  pulseCount = 0;
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastPrintTime;

  if (elapsed >= PRINT_INTERVAL_MS) {
    // Safely grab pulse count
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();

    float dt = elapsed / 1000.0f;

    // One pulse = one revolution
    float rps = count / dt;
    float rpm = rps * 60.0f;

    float rpmAvg = movingAverageRPM(rpm);
    float velocity = velocityFromRPM(rpmAvg);

    // CSV output
    Serial.print(now);
    Serial.print(",");
    Serial.print(count);
    Serial.print(",");
    Serial.print(rpm, 1);
    Serial.print(",");
    Serial.println(velocity, 3);

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(count);
        file.print(",");
        file.print(rpm, 1);
        file.print(",");
        file.println(velocity, 3);
        file.close();
      }
    }

    lastPrintTime = now;
  }
}
