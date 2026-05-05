// MLX90316 analog weather vane on Teensy 4.0
// Assumes sensor OUT is scaled to Teensy-safe range with a voltage divider
// Potential divider: 18k top, 30k bottom.

#include <SPI.h>
#include <SD.h>

const int vanePin = A1;  // Teensy input pin (moved from A0 to A1)

// Teensy ADC settings
const float Vref = 3.3f;
const int ADC_BITS = 12;                  // Teensy read resolution
const int ADC_MAX = (1 << ADC_BITS) - 1;  // 4095 for 12-bit

// Measured/calibrated voltage range at the Teensy pin.
// REPLACE after calibration!
float Vmin_meas = 0.094f;
float Vmax_meas = 3.000f;

// Measured/calibrated North Offset
// The raw angle reads 230 deg when vane points north, set northOffset = -230.
float northOffsetDeg = 0.0f;

// Set true if angle increases in the wrong direction when the vane rotates.
bool reverseDirection = false;

// Moving-average smoothing (avg last 10 samples to reduce flutter)
const int N_SAMPLES = 10;
int sampleBuffer[N_SAMPLES];
int sampleIndex = 0;
long sampleSum = 0;
bool bufferFilled = false;

// SD logging
static bool sdReady = false;
static char logFilename[20];

float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

// Function to wrap angle
float wrap360(float angleDeg) {
  while (angleDeg >= 360.0f) angleDeg -= 360.0f;
  while (angleDeg < 0.0f) angleDeg += 360.0f;
  return angleDeg;
}

// initialize first 10 readings + fill n/a data
int readSmoothedADC() {
  int raw = analogRead(vanePin);

  // Fill buffer first time
  if (!bufferFilled) {
    sampleSum = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
      sampleBuffer[i] = raw;
      sampleSum += raw;
    }
    bufferFilled = true;
    sampleIndex = 0;
    return raw;
  }

  // Running moving average
  sampleSum -= sampleBuffer[sampleIndex];
  sampleBuffer[sampleIndex] = raw;
  sampleSum += raw;

  sampleIndex++;
  if (sampleIndex >= N_SAMPLES) sampleIndex = 0;

  return (int)(sampleSum / N_SAMPLES);
}

float adcToVoltage(int adc) {
  return (adc * Vref) / ADC_MAX;
}

// Convert Voltage to Angle
float voltageToAngle(float volts) {
  volts = clampf(volts, Vmin_meas, Vmax_meas);

  float angle = 360.0f * (volts - Vmin_meas) / (Vmax_meas - Vmin_meas);

  if (reverseDirection) {
    angle = 360.0f - angle;
  }

  angle = angle + northOffsetDeg;
  return wrap360(angle);
}

void setup() {
  Serial.begin(115200);

  // Disable motor pins -- they float on boot and the H-bridge will run them.
  const uint8_t motorPins[] = {3, 4, 5, 6, 22, 23};
  for (uint8_t i = 0; i < sizeof(motorPins); i++) {
    pinMode(motorPins[i], OUTPUT);
    digitalWrite(motorPins[i], LOW);
  }

  analogReadResolution(ADC_BITS);

  // Initialize SD card
  Serial.print("Initializing SD Card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "VANE%04d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,ADC,Voltage,Angle");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  delay(500);
  Serial.println("MLX90316 Analog Weather Vane");
  Serial.println("Time,ADC,Voltage,Angle");
}

void loop() {
  int adc = readSmoothedADC();
  float volts = adcToVoltage(adc);
  float angleDeg = voltageToAngle(volts);

  // Print in CSV format
  Serial.print(millis());
  Serial.print(",");
  Serial.print(adc);
  Serial.print(",");
  Serial.print(volts, 4);
  Serial.print(",");
  Serial.println(angleDeg, 2);

  // Log to SD
  if (sdReady) {
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.print(millis());
      file.print(",");
      file.print(adc);
      file.print(",");
      file.print(volts, 4);
      file.print(",");
      file.println(angleDeg, 2);
      file.close();
    }
  }

  delay(200);  // 5 Hz
}
