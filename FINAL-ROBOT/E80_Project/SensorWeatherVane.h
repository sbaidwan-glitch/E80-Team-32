#ifndef __SENSOR_WEATHER_VANE_H__
#define __SENSOR_WEATHER_VANE_H__

#include <Arduino.h>
#include "DataSource.h"

// MLX90316 analog output, read through a voltage divider (18k/30k).
// Teensy pin 14 = A0 on the MTB (A00 column in ADCSampler).
#define VANE_PIN A0

class SensorWeatherVane : public DataSource {
public:
  SensorWeatherVane(void);

  void init(void);
  void read(void);

  String printState(void);
  size_t writeDataBytes(unsigned char * buffer, size_t idx);

  float angleDeg;   // wind direction in degrees [0, 360)
  float voltage;    // measured voltage at Teensy pin

  int lastExecutionTime = -1;

private:
  static float wrap360(float a);
  int readSmoothedADC(void);

  // Calibration: measured voltage range at Teensy pin after divider.
  // Update these after calibration with the test sketch.
  static constexpr float VMIN_MEAS = 0.094f;
  static constexpr float VMAX_MEAS = 3.000f;

  // North offset: if raw angle reads X deg when vane points north,
  // set this to -X so that north reads 0.
  static constexpr float NORTH_OFFSET_DEG = 0.0f;

  // Set true if angle increases in the wrong rotational direction.
  static constexpr bool REVERSE_DIRECTION = false;

  // ADC settings
  static constexpr int ADC_BITS = 12;
  static constexpr int ADC_MAX  = (1 << ADC_BITS) - 1;
  static constexpr float VREF   = 3.3f;

  // Moving-average smoothing
  static const int N_SAMPLES = 10;
  int sampleBuffer[N_SAMPLES];
  int sampleIndex;
  long sampleSum;
  bool bufferFilled;
};

#endif
