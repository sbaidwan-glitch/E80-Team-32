

#ifndef __SENSOR_ANEMOMETER_H__
#define __SENSOR_ANEMOMETER_H__

#include <Arduino.h>
#include "DataSource.h"

// AH9246 hall effect sensor output connects to this pin.
// Teensy pin 14 (A0 on the MTB). Used as digital interrupt.
#define ANEMOMETER_PIN 14

// Minimum microseconds between valid pulses (debounce/noise filter)
#define MIN_PULSE_SPACING_US 10000

// Moving average window size for RPM smoothing
#define ANEMO_N_AVG 5

/*
 * SensorAnemometer counts pulses from the AH9246 hall effect sensor
 * to measure cup anemometer rotation frequency, then converts to
 * wind speed using a calibration curve.
 *
 * The AH9246 output goes LOW when the magnet passes (FALLING edge).
 * Debounce filtering rejects pulses closer than MIN_PULSE_SPACING_US.
 *
 * Calibration: velocity = (rpm - B_CAL) / A_CAL
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

  // Moving average for RPM
  float rpmHist[ANEMO_N_AVG];
  int rpmIdx;
  bool rpmFilled;
  float movingAverageRPM(float newVal);

  // Calibration constants (from wind tunnel testing)
  // velocity = (rpm - B_CAL) / A_CAL
  static constexpr float A_CAL = 21.1f;
  static constexpr float B_CAL = -1.71f;
};

#endif
