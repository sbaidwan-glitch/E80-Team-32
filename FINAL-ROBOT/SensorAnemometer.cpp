
#include "SensorAnemometer.h"
#include "Printer.h"
extern Printer printer;

static volatile unsigned long g_pulseCount = 0;

// increments on each rising edge
static void anemometerISR(void) {
  g_pulseCount++;
}

SensorAnemometer::SensorAnemometer(void)
  : DataSource("anemo_freq,wind_speed", "float,float") // logged columns
{
  frequency = 0.0;
  windSpeed = 0.0;
  lastReadTime = 0;
}

void SensorAnemometer::init(void) {
  pinMode(ANEMOMETER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, RISING);
  lastReadTime = millis();
  g_pulseCount = 0;
  Serial.println("Anemometer init OK (pin " + String(ANEMOMETER_PIN) + ")");
}

void SensorAnemometer::read(void) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastReadTime;

  if (elapsed == 0) return;  // avoid divide-by-zero on first call

  noInterrupts();
  unsigned long count = g_pulseCount;
  g_pulseCount = 0;
  interrupts();

  // Frequency in Hz = pulses / elapsed seconds
  frequency = (float)count / ((float)elapsed / 1000.0);

  // TODO: update CAL_SLOPE and CAL_OFFSET after wind tunnel calibration
  windSpeed = CAL_SLOPE * frequency + CAL_OFFSET;

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
