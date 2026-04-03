

#ifndef __SENSOR_ANEMOMETER_H__
#define __SENSOR_ANEMOMETER_H__

#include <Arduino.h>
#include "DataSource.h"

// AH9246 hall effect sensor output connects to this pin.
// This is Teensy pin 16 (also labeled A2 on the MTB).
// Pin chosen in Research Project proposal, Figure 2.
// Note: ADCSampler also reads pin 16 as an analog channel (A02),
// which will return noise since we configure it as digital here.
// That A02 column in the log can be ignored.
#define ANEMOMETER_PIN 16

/*
 * SensorAnemometer counts pulses from the AH9246 hall effect sensor
 * to measure cup anemometer rotation frequency, then converts to
 * wind speed using a calibration curve.
 *
 * The AH9246 outputs a digital pulse each time the magnet on the
 * rotating shaft passes by. We count rising edges via interrupt
 * and compute frequency each loop period.
 *
 * Calibration: wind_speed = CAL_SLOPE * frequency + CAL_OFFSET
 * These values must be updated after wind tunnel calibration.
 */
class SensorAnemometer : public DataSource {
public:
  SensorAnemometer(void);

  // Set up pin and attach interrupt
  void init(void);

  // Compute frequency from pulse count since last read
  void read(void);

  String printState(void);
  size_t writeDataBytes(unsigned char * buffer, size_t idx);

  float frequency;    // rotation frequency in Hz
  float windSpeed;    // converted wind speed in m/s

  int lastExecutionTime = -1;

private:
  unsigned long lastReadTime;

  // Calibration constants (update after wind tunnel testing)
  // wind_speed = CAL_SLOPE * frequency + CAL_OFFSET
  static constexpr float CAL_SLOPE  = 1.0;  // placeholder [m/s per Hz]
  static constexpr float CAL_OFFSET = 0.0;  // placeholder [m/s]
};

#endif
