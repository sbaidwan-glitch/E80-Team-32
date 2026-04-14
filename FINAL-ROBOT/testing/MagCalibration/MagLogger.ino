/*
  Magnetometer Data Logger for E80 Team 32 - Teensy 4.0

  Logs raw magnetometer (mx, my, mz) and heading data from the
  LSM303AGR IMU to serial monitor and SD card.

  Use this to:
    1. Verify the IMU is working (before calibration)
    2. Collect uncalibrated mag data while spinning 360 degrees
    3. Collect calibrated mag data after updating SensorIMU.h
    4. Compare heading before/after calibration

  Hardware:
    - IMU (LSM303AGR) on I2C bus (SDA=18, SCL=19)
    - SD card inserted for logging

  Instructions:
    1. Upload this sketch
    2. Open Serial Monitor at 115200 baud
    3. Go OUTSIDE, far from buildings
    4. Hold the robot upright and slowly spin 360 degrees over ~10 seconds
    5. Pull SD card and open the CSV in MATLAB

  Output columns: Time_ms, magX, magY, magZ, heading, roll, pitch
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <LSM303AGR_ACC_Sensor.h>
#include <LSM303AGR_MAG_Sensor.h>

// How often to sample and print (ms)
// 50ms = 20 Hz, good for capturing a smooth 360 spin in ~10s
#define SAMPLE_INTERVAL_MS 50

typedef struct LSM303AGR {
  TwoWire *dev_i2c = &Wire;
  LSM303AGR_ACC_Sensor *Acc = new LSM303AGR_ACC_Sensor(dev_i2c);
  LSM303AGR_MAG_Sensor *Mag = new LSM303AGR_MAG_Sensor(dev_i2c);
} LSM303AGR;

LSM303AGR myIMU;

static unsigned long lastSampleTime = 0;
static bool sdReady = false;
static char logFilename[20];
static unsigned long sampleCount = 0;

// ---- Calibration values ----
// Replace these with your values from Mag_Calibration.m
// Until calibrated, leave as-is (minimal correction)
float mag_offsets[3]       = { 1.00, 1.00, 1.00 };
float mag_ironcomp[3][3]   = { {  1.00,  0.00,  0.00 },
                               {  0.00,  1.00,  0.00 },
                               {  0.00,  0.00,  1.00 } };
float accel_offsets[3]     = { 1.0, 1.0, 1.0 };

// Set to true after you paste in real calibration values
#define CALIBRATION_APPLIED false

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  Serial.println("=== E80 Magnetometer Logger ===");
  if (CALIBRATION_APPLIED) {
    Serial.println("Mode: CALIBRATED (using your correction values)");
  } else {
    Serial.println("Mode: UNCALIBRATED (raw data, placeholder offsets)");
  }
  Serial.println();

  // Initialize I2C and IMU
  myIMU.dev_i2c->begin();

  bool imuOk = (myIMU.Acc->begin()   == LSM303AGR_ACC_STATUS_OK
             && myIMU.Acc->Enable()   == LSM303AGR_ACC_STATUS_OK
             && myIMU.Mag->begin()    == LSM303AGR_MAG_STATUS_OK
             && myIMU.Mag->Enable()   == LSM303AGR_MAG_STATUS_OK);

  if (imuOk) {
    Serial.println("IMU initialized OK");
  } else {
    Serial.println("IMU FAILED! Check I2C wiring (SDA=18, SCL=19)");
  }

  // Initialize SD card
  Serial.print("Initializing SD card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "MAG_%04d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,magX_raw,magY_raw,magZ_raw,magX_cal,magY_cal,magZ_cal,heading,roll,pitch");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  Serial.println();
  Serial.println("Hold robot upright, go outside, and slowly spin 360 degrees.");
  Serial.println();
  Serial.println("Time_ms,magX_raw,magY_raw,magZ_raw,magX_cal,magY_cal,magZ_cal,heading,roll,pitch");

  lastSampleTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    sampleCount++;

    // Read accelerometer
    int32_t raw_acc[3];
    myIMU.Acc->GetAxes(raw_acc);
    float ax = (float)raw_acc[0] - accel_offsets[0];
    float ay = -(float)raw_acc[1] - accel_offsets[1];  // negate Y for RHR
    float az = (float)raw_acc[2] - accel_offsets[2];

    // Read magnetometer
    int32_t raw_mag[3];
    myIMU.Mag->GetAxes(raw_mag);
    float mx_raw = (float)raw_mag[0] * 0.1;         // mGauss to uTesla
    float my_raw = -(float)raw_mag[1] * 0.1;        // negate Y for RHR
    float mz_raw = (float)raw_mag[2] * 0.1;

    // Apply hard iron offset
    float mx_off = mx_raw - mag_offsets[0];
    float my_off = my_raw - mag_offsets[1];
    float mz_off = mz_raw - mag_offsets[2];

    // Apply soft iron compensation matrix
    float mx_cal = mx_off * mag_ironcomp[0][0] + my_off * mag_ironcomp[0][1] + mz_off * mag_ironcomp[0][2];
    float my_cal = mx_off * mag_ironcomp[1][0] + my_off * mag_ironcomp[1][1] + mz_off * mag_ironcomp[1][2];
    float mz_cal = mx_off * mag_ironcomp[2][0] + my_off * mag_ironcomp[2][1] + mz_off * mag_ironcomp[2][2];

    // Compute tilt-compensated heading (same formula as SensorIMU.cpp)
    float roll  = atan2(ay, az);
    float pitch = (ay * sin(roll) + az * cos(roll) == 0)
                  ? (ax > 0 ? PI / 2.0 : -PI / 2.0)
                  : atan(-ax / (ay * sin(roll) + az * cos(roll)));

    float heading = -atan2(mz_cal * sin(roll) - my_cal * cos(roll),
                           mx_cal * cos(pitch) +
                           my_cal * sin(pitch) * sin(roll) +
                           mz_cal * sin(pitch) * cos(roll));

    // Convert to degrees
    float roll_deg    = roll * 180.0 / PI;
    float pitch_deg   = pitch * 180.0 / PI;
    float heading_deg = heading * 180.0 / PI;

    // Print CSV line
    Serial.print(now);
    Serial.print(",");
    Serial.print(mx_raw, 2);
    Serial.print(",");
    Serial.print(my_raw, 2);
    Serial.print(",");
    Serial.print(mz_raw, 2);
    Serial.print(",");
    Serial.print(mx_cal, 2);
    Serial.print(",");
    Serial.print(my_cal, 2);
    Serial.print(",");
    Serial.print(mz_cal, 2);
    Serial.print(",");
    Serial.print(heading_deg, 1);
    Serial.print(",");
    Serial.print(roll_deg, 1);
    Serial.print(",");
    Serial.println(pitch_deg, 1);

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(mx_raw, 2);
        file.print(",");
        file.print(my_raw, 2);
        file.print(",");
        file.print(mz_raw, 2);
        file.print(",");
        file.print(mx_cal, 2);
        file.print(",");
        file.print(my_cal, 2);
        file.print(",");
        file.print(mz_cal, 2);
        file.print(",");
        file.print(heading_deg, 1);
        file.print(",");
        file.print(roll_deg, 1);
        file.print(",");
        file.println(pitch_deg, 1);
        file.close();
      }
    }
  }
}
