/*
  Phake Lake Test - Raw Power Version
  E80 Team 32

  Motors A & B drive for 1 minute forward, then 1 minute reverse.
  Motor C is OFF. No heading hold.
  Every 5 seconds of driving, motors pause for 10 seconds to collect
  clean sensor data without motor noise.

  Starts after a 40-second countdown (no button press).
  Logs: IMU, GPS, motors, anemometer, weather vane to SD (binary log).

  WARNING: Motors will spin after the 40s countdown.
  Keep hands and cables clear of propellers.

  To tune individual motor strength, change MOTOR_A_POWER and
  MOTOR_B_POWER below (lines 30-31). Use this if one motor is
  stronger than the other to keep the boat going roughly straight.
*/

#include <Arduino.h>
#include <Wire.h>
#include <Pinouts.h>
#include <TimingOffsets.h>
#include <SensorGPS.h>
#include <SensorIMU.h>
#include <MotorDriver.h>
#include <Logger.h>
#include <Printer.h>
#define UartSerial Serial1
#include <GPSLockLED.h>

#include "SensorAnemometer.h"
#include "SensorWeatherVane.h"

// Drive duration (total driving time per direction, not counting pauses)
#define DRIVE_DURATION_MS   60000   // 1 minute of actual driving

// Motor power for A and B (0-255). Adjust these independently
// if one motor is stronger than the other.
#define MOTOR_A_POWER       -255     // <-- change this for left motor
#define MOTOR_B_POWER       -16     // <-- change this for right motor

// Drive/pause cycle: drive 5s, pause 10s for clean sensor readings
#define DRIVE_ON_MS         5000    // motors on for 5 seconds
#define DRIVE_PAUSE_MS      10000   // motors off for 10 seconds

/////////////////////////* Global Variables *////////////////////////

MotorDriver motor_driver;
SensorGPS gps;
Adafruit_GPS GPS(&UartSerial);
SensorIMU imu;
Logger logger;
Printer printer;
GPSLockLED led;

SensorAnemometer anemometer;
SensorWeatherVane weatherVane; //320 when pointing fwd, 2.555V

int loopStartTime;
int currentTime;
bool reversing = false;
bool done = false;
unsigned long totalDriveTime = 0;       // accumulated actual driving time in current phase
unsigned long cycleStartTime = 0;       // when current on/off cycle began
unsigned long lastDriveCheck = 0;       // for tracking real elapsed drive time
bool motorsPaused = false;              // true during 10s pause windows

////////////////////////* Setup *////////////////////////////////

void setup() {
  logger.include(&imu);
  logger.include(&gps);
  logger.include(&motor_driver);
  logger.include(&anemometer);
  logger.include(&weatherVane);
  logger.init();

  printer.init();
  imu.init();
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  anemometer.init();
  weatherVane.init();

  printer.printMessage("RAW POWER - 40 second countdown...", 10);

  // 40-second countdown before motors start
  for (int i = 15; i > 0; i--) {
    Serial.print("Starting in ");
    Serial.print(i);
    Serial.println(" seconds...");
    delay(1000);
  }
  Serial.println("GO - FORWARD!");

  loopStartTime = millis();
  printer.lastExecutionTime    = loopStartTime - LOOP_PERIOD + PRINTER_LOOP_OFFSET;
  imu.lastExecutionTime        = loopStartTime - LOOP_PERIOD + IMU_LOOP_OFFSET;
  anemometer.lastExecutionTime = loopStartTime - LOOP_PERIOD + 80;
  weatherVane.lastExecutionTime= loopStartTime - LOOP_PERIOD + 85;
  logger.lastExecutionTime     = loopStartTime - LOOP_PERIOD + LOGGER_LOOP_OFFSET;

  cycleStartTime = millis();
  lastDriveCheck = millis();
  totalDriveTime = 0;
  motorsPaused = false;

  // Start driving forward (no motor C)
  motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, 0);
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

  // ---- Motor control with 5s on / 10s off cycling ----
  if (!done) {
    int dirA = reversing ? -MOTOR_A_POWER : MOTOR_A_POWER;
    int dirB = reversing ? -MOTOR_B_POWER : MOTOR_B_POWER;

    unsigned long cycleElapsed = (unsigned long)(currentTime - cycleStartTime);

    if (!motorsPaused) {
      // Motors are ON — drive for 5 seconds
      motor_driver.drive(dirA, dirB, 0);
      // Track real elapsed driving time (only counts when motors are on)
      totalDriveTime += (unsigned long)(currentTime - lastDriveCheck);
      lastDriveCheck = currentTime;

      if (cycleElapsed >= DRIVE_ON_MS) {
        // Pause motors for sensor collection
        motorsPaused = true;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;  // freeze drive time counter
        motor_driver.drive(0, 0, 0);
        printer.printMessage("PAUSE - collecting data (10s)", 10);
      }
    } else {
      // Motors are OFF — pause for 10 seconds
      motor_driver.drive(0, 0, 0);

      if (cycleElapsed >= DRIVE_PAUSE_MS) {
        // Resume driving
        motorsPaused = false;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;  // reset so pause time isn't counted
        motor_driver.drive(dirA, dirB, 0);
        printer.printMessage(reversing ? "DRIVING (reverse)" : "DRIVING (forward)", 10);
      }
    }

    // Check if we've accumulated enough driving time for this phase
    if (totalDriveTime >= DRIVE_DURATION_MS) {
      if (!reversing) {
        // Switch to reverse
        reversing = true;
        totalDriveTime = 0;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;
        motorsPaused = false;
        motor_driver.drive(-MOTOR_A_POWER, -MOTOR_B_POWER, 0);
        printer.printMessage("REVERSING - 1 minute reverse", 10);
      } else {
        // All done
        done = true;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("DONE - forward + reverse complete", 10);
      }
    }
  } else {
    motor_driver.drive(0, 0, 0);
  }

  // Read IMU
  if (currentTime - imu.lastExecutionTime > LOOP_PERIOD) {
    imu.lastExecutionTime = currentTime;
    imu.read();
  }

  // Read GPS (every iteration)
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
