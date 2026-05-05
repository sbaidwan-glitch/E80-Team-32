/*
  Phake Lake Test - Heading Hold Version
  E80 Team 32

  P-controller heading hold for 1 minute, then 1 minute reverse.
  Press button to start.
  Logs: IMU, GPS, motors, anemometer, weather vane to SD (binary log).
  No photodiodes (no VEML6030, no BPW34/ADCSampler).

  1. Upload this sketch
  2. Place robot in water
  3. Point robot in desired direction
  4. Press the button on the motherboard
  5. Robot drives forward for 1 minute (heading hold)
  6. Robot reverses all motors for 1 minute
  7. Robot stops
*/

#include <Arduino.h>
#include <Wire.h>
#include <Pinouts.h>
#include <TimingOffsets.h>
#include <SensorGPS.h>
#include <SensorIMU.h>
#include <ButtonSampler.h>
#include <MotorDriver.h>
#include <Logger.h>
#include <Printer.h>
#define UartSerial Serial1
#include <GPSLockLED.h>

#include "SensorAnemometer.h"
#include "SensorWeatherVane.h"

// Drive duration
#define DRIVE_DURATION_MS   60000   // 1 minute

// Motor power (0-255)
#define BASE_POWER          150

// Heading P-controller gain
#define KP_HEADING          2.0

// States
#define STATE_IDLE    0
#define STATE_DRIVE   1
#define STATE_REVERSE 2
#define STATE_DONE    3

/////////////////////////* Global Variables *////////////////////////

MotorDriver motor_driver;
SensorGPS gps;
Adafruit_GPS GPS(&UartSerial);
ButtonSampler button_sampler;
SensorIMU imu;
Logger logger;
Printer printer;
GPSLockLED led;

SensorAnemometer anemometer;
SensorWeatherVane weatherVane;

int loopStartTime;
int currentTime;

int navState = STATE_IDLE;
float targetHeading = 0.0;
unsigned long driveStartTime = 0;

////////////////////////* Setup *////////////////////////////////

void setup() {
  // Register data sources for binary logging
  logger.include(&imu);             // rollIMU,pitchIMU,headingIMU,accelX,accelY,accelZ,magX,magY,magZ
  logger.include(&gps);             // lat,lon,nsats
  logger.include(&motor_driver);    // motorA,motorB,motorC
  logger.include(&anemometer);      // anemo_freq,wind_speed
  logger.include(&weatherVane);     // vane_angle,vane_voltage
  logger.init();

  printer.init();
  button_sampler.init();
  imu.init();
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  anemometer.init();
  weatherVane.init();

  printer.printMessage("Phake Lake HEADING test ready. Press button to start.", 10);

  loopStartTime = millis();
  printer.lastExecutionTime        = loopStartTime - LOOP_PERIOD + PRINTER_LOOP_OFFSET;
  imu.lastExecutionTime            = loopStartTime - LOOP_PERIOD + IMU_LOOP_OFFSET;
  button_sampler.lastExecutionTime = loopStartTime - LOOP_PERIOD + BUTTON_LOOP_OFFSET;
  anemometer.lastExecutionTime     = loopStartTime - LOOP_PERIOD + 80;
  weatherVane.lastExecutionTime    = loopStartTime - LOOP_PERIOD + 85;
  logger.lastExecutionTime         = loopStartTime - LOOP_PERIOD + LOGGER_LOOP_OFFSET;
}

//////////////////////////////* Loop */////////////////////////

void loop() {
  currentTime = millis();

  // Print status
  if (currentTime - printer.lastExecutionTime > LOOP_PERIOD) {
    printer.lastExecutionTime = currentTime;
    printer.printValue(0, gps.printState());
    printer.printValue(1, imu.printRollPitchHeading());
    printer.printValue(2, motor_driver.printState());
    printer.printValue(3, anemometer.printState());
    printer.printValue(4, weatherVane.printState());
    printer.printValue(5, logger.printState());
    printer.printToSerial();
  }

  // State machine
  if (navState == STATE_IDLE) {
    motor_driver.drive(0, 0, 0);
    if (button_sampler.buttonState) {
      targetHeading = imu.state.heading;
      navState = STATE_DRIVE;
      driveStartTime = currentTime;
      printer.printMessage("GO! Heading: " + String(targetHeading, 1) + " deg", 10);
    }
  }

  else if (navState == STATE_DRIVE) {
    // P-controller
    float headingError = targetHeading - imu.state.heading;
    while (headingError >  180.0) headingError -= 360.0;
    while (headingError < -180.0) headingError += 360.0;

    int correction = (int)(KP_HEADING * headingError);
    int uL = constrain(BASE_POWER + correction, -255, 255);
    int uR = constrain(BASE_POWER - correction, -255, 255);
    motor_driver.drive(uL, uR, BASE_POWER);

    // Switch to reverse after 1 minute
    if ((unsigned long)(currentTime - driveStartTime) >= DRIVE_DURATION_MS) {
      navState = STATE_REVERSE;
      driveStartTime = currentTime;
      motor_driver.drive(-BASE_POWER, -BASE_POWER, -BASE_POWER);
      printer.printMessage("REVERSING - all motors reverse for 1 minute", 10);
    }
  }

  else if (navState == STATE_REVERSE) {
    motor_driver.drive(-BASE_POWER, -BASE_POWER, -BASE_POWER);

    // Stop after 1 minute of reverse
    if ((unsigned long)(currentTime - driveStartTime) >= DRIVE_DURATION_MS) {
      navState = STATE_DONE;
      motor_driver.drive(0, 0, 0);
      printer.printMessage("DONE - forward + reverse complete", 10);
    }
  }

  else {
    motor_driver.drive(0, 0, 0);
  }

  // Read button
  if (currentTime - button_sampler.lastExecutionTime > LOOP_PERIOD) {
    button_sampler.lastExecutionTime = currentTime;
    button_sampler.updateState();
  }

  // Read IMU
  if (currentTime - imu.lastExecutionTime > LOOP_PERIOD) {
    imu.lastExecutionTime = currentTime;
    imu.read();
  }

  // Read GPS (every iteration — must consume serial bytes)
  gps.read(&GPS);

  // Read anemometer
  if (currentTime - anemometer.lastExecutionTime > LOOP_PERIOD) {
    anemometer.lastExecutionTime = currentTime;
    anemometer.read();
  }

  // Read weather vane
  if (currentTime - weatherVane.lastExecutionTime > LOOP_PERIOD) {
    weatherVane.lastExecutionTime = currentTime;
    weatherVane.read();
  }

  // GPS lock LED
  if (currentTime - led.lastExecutionTime > LOOP_PERIOD) {
    led.lastExecutionTime = currentTime;
    led.flashLED(&gps.state);
  }

  // Log to SD
  if (currentTime - logger.lastExecutionTime > LOOP_PERIOD && logger.keepLogging) {
    logger.lastExecutionTime = currentTime;
    logger.log();
  }
}
