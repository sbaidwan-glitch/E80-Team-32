
#include "SensorAnemometer.h"
#include "Printer.h"
extern Printer printer;

static volatile unsigned long g_pulseCount = 0;
static volatile unsigned long g_lastPulseMicros = 0;

// Interrupt: counts magnet passes with debounce
static void anemometerISR(void) {
  unsigned long nowUs = micros();
  if (nowUs - g_lastPulseMicros >= MIN_PULSE_SPACING_US) {
    g_pulseCount++;
    g_lastPulseMicros = nowUs;
  }
}

SensorAnemometer::SensorAnemometer(void)
  : DataSource("anemo_freq,wind_speed", "float,float")
{
  frequency = 0.0;
  windSpeed = 0.0;
  lastReadTime = 0;
  rpmIdx = 0;
  rpmFilled = false;
  for (int i = 0; i < ANEMO_N_AVG; i++) rpmHist[i] = 0;
}

void SensorAnemometer::init(void) {
  pinMode(ANEMOMETER_PIN, INPUT);
  // Sensor goes LOW when magnet passes -> FALLING edge
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, FALLING);
  lastReadTime = millis();
  g_pulseCount = 0;
  g_lastPulseMicros = 0;
  Serial.println("Anemometer init OK (pin " + String(ANEMOMETER_PIN) + ")");
}

float SensorAnemometer::movingAverageRPM(float newVal) {
  rpmHist[rpmIdx] = newVal;
  rpmIdx++;

  if (rpmIdx >= ANEMO_N_AVG) {
    rpmIdx = 0;
    rpmFilled = true;
  }

  int n = rpmFilled ? ANEMO_N_AVG : rpmIdx;
  if (n <= 0) return newVal;

  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    sum += rpmHist[i];
  }
  return sum / n;
}

void SensorAnemometer::read(void) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastReadTime;

  if (elapsed == 0) return;

  noInterrupts();
  unsigned long count = g_pulseCount;
  g_pulseCount = 0;
  interrupts();

  // Frequency in Hz = pulses / elapsed seconds
  frequency = (float)count / ((float)elapsed / 1000.0);

  // Convert to RPM (one pulse = one revolution), then smooth
  float rpm = frequency * 60.0f;
  float rpmAvg = movingAverageRPM(rpm);

  // velocity = (rpm - B_CAL) / A_CAL
  windSpeed = (rpmAvg - B_CAL) / A_CAL;

  lastReadTime = now;
}

String SensorAnemometer::printState(void) {
  return "Anemo: " + String(frequency, 1) + " Hz, " + String(windSpeed, 1) + " m/s";
}

size_t SensorAnemometer::writeDataBytes(unsigned char * buffer, size_t idx) {
  float * data_slot = (float *) &buffer[idx];
  data_slot[0] = frequency;
  data_slot[1] = windSpeed;
  return idx + 2 * sizeof(float);
}
