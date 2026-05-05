#include "SensorWeatherVane.h"
#include "Printer.h"
extern Printer printer;

SensorWeatherVane::SensorWeatherVane(void)
  : DataSource("vane_angle,vane_voltage", "float,float")
{
  angleDeg = 0.0;
  voltage = 0.0;
  sampleIndex = 0;
  sampleSum = 0;
  bufferFilled = false;
}

void SensorWeatherVane::init(void) {
  analogReadResolution(ADC_BITS);
  pinMode(VANE_PIN, INPUT);
  Serial.println("WeatherVane init OK (pin " + String(VANE_PIN) + ")");
}

void SensorWeatherVane::read(void) {
  int adc = readSmoothedADC();
  voltage = (float)adc * VREF / ADC_MAX;

  // Clamp to calibrated range
  float v = voltage;
  if (v < VMIN_MEAS) v = VMIN_MEAS;
  if (v > VMAX_MEAS) v = VMAX_MEAS;

  float angle = 360.0f * (v - VMIN_MEAS) / (VMAX_MEAS - VMIN_MEAS);

  if (REVERSE_DIRECTION) {
    angle = 360.0f - angle;
  }

  angleDeg = wrap360(angle + NORTH_OFFSET_DEG);
}

String SensorWeatherVane::printState(void) {
  return "Vane: " + String(angleDeg, 1) + " deg (" + String(voltage, 3) + " V)";
}

size_t SensorWeatherVane::writeDataBytes(unsigned char * buffer, size_t idx) {
  float * data_slot = (float *) &buffer[idx];
  data_slot[0] = angleDeg;
  data_slot[1] = voltage;
  return idx + 2 * sizeof(float);
}

float SensorWeatherVane::wrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a < 0.0f)    a += 360.0f;
  return a;
}

int SensorWeatherVane::readSmoothedADC(void) {
  int raw = analogRead(VANE_PIN);

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

  sampleSum -= sampleBuffer[sampleIndex];
  sampleBuffer[sampleIndex] = raw;
  sampleSum += raw;

  sampleIndex++;
  if (sampleIndex >= N_SAMPLES) sampleIndex = 0;

  return (int)(sampleSum / N_SAMPLES);
}
