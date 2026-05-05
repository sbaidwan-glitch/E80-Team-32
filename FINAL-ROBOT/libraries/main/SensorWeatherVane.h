#ifndef __SENSOR_WEATHER_VANE_H__
#define __SENSOR_WEATHER_VANE_H__

#include <Arduino.h>
#include "DataSource.h"

// MLX90316 analog output, read through an 18k/30k voltage divider.
// Teensy pin 15 = A1 (MTB column A01).
#define VANE_PIN A1

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

  // Measured voltage range at Teensy pin after divider. Update from bench cal.
  static constexpr float VMIN_MEAS = 1.50f;
  static constexpr float VMAX_MEAS = 2.10f;

  // If raw angle reads X deg when vane points north, set NORTH_OFFSET_DEG = -X.
  static constexpr float NORTH_OFFSET_DEG = 0.0f;

  // Set true if angle increases in the wrong rotational direction.
  static constexpr bool REVERSE_DIRECTION = false;

  static constexpr int ADC_BITS = 12;
  static constexpr int ADC_MAX  = (1 << ADC_BITS) - 1;
  static constexpr float VREF   = 3.3f;

  static const int N_SAMPLES = 10;
  int sampleBuffer[N_SAMPLES];
  int sampleIndex;
  long sampleSum;
  bool bufferFilled;
};

#endif
