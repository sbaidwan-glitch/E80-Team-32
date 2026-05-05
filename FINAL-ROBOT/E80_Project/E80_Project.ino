/*
E80 Project - Team 32
Surface vehicle measuring wind speed, wind direction, and solar irradiance
vs. distance from shore at Dana Point.

Based on Default_Robot.ino from E80 Github.

Sensors:
  - VEML6030: ambient light (I2C, pins 18/19)
  - AH9246: anemometer hall effect (digital interrupt, pin 14)
  - MLX90316 analog weather vane (analog, pin 15 = A01)
  - BPW34: photodiode + TIA (analog, logged by ADCSampler)

Navigation:
  Point robot away from shore, press button to start.
  Robot drives straight, stops at each station to collect data, then drives to the next station.

Pin usage on MTB protoboard connector (J1):
  Pin 14 (A00) - AH9246 anemometer digital pulses
  Pin 15 (A01) - MLX90316 weather vane analog output
  Pin 18 (SDA) - VEML6030 I2C data
  Pin 19 (SCL) - VEML6030 I2C clock

*/

#include <Arduino.h>
#include <Wire.h>
#include <Pinouts.h>
#include <TimingOffsets.h>
#include <SensorGPS.h>
#include <SensorIMU.h>
#include <XYStateEstimator.h>
#include <ADCSampler.h>
#include <ErrorFlagSampler.h>
#include <ButtonSampler.h>
#include <MotorDriver.h>
#include <Logger.h>
#include <Printer.h>
#define UartSerial Serial1
#include <GPSLockLED.h>

#include "SensorVEML6030.h"
#include "SensorAnemometer.h"
#include "SensorWeatherVane.h"

// Number of measurement stations (stops)
#define NUM_STATIONS        5

// Time driving between stations (ms). At ~0.3 m/s this gives ~36m spacing.
#define DRIVE_DURATION_MS   120000   // 2 minutes

// Time stopped at each station collecting data (ms)
#define STATION_DURATION_MS 300000   // 5 minutes

// Base motor power for forward driving (0-255)
#define BASE_POWER          150

// Proportional gain for staying on course
#define KP_HEADING          2.0

// Navigation machine states
#define NAV_IDLE    0  // waiting for button press
#define NAV_DRIVE   1  // heading-hold driving
#define NAV_STATION 2  // stopped, collecting data
#define NAV_DONE    3  // all stations complete

/////////////////////////* Global Variables *////////////////////////

MotorDriver motor_driver;
XYStateEstimator state_estimator;
SensorGPS gps;
Adafruit_GPS GPS(&UartSerial);
ADCSampler adc;
ErrorFlagSampler ef;
ButtonSampler button_sampler;
SensorIMU imu;
Logger logger;
Printer printer;
GPSLockLED led;

// Project sensors
SensorVEML6030 veml6030;
SensorAnemometer anemometer;
SensorWeatherVane weatherVane;

// loop start recorder
int loopStartTime;
int currentTime;
volatile bool EF_States[NUM_FLAGS] = {1, 1, 1};

// Navigation state
int navState = NAV_IDLE;
float targetHeading = 0.0;        // degrees, captured on button press
unsigned long stateStartTime = 0; // when current nav state began
int stationsCompleted = 0;
int navLastExecutionTime;

////////////////////////* Setup *////////////////////////////////

void setup() {


  logger.include(&imu);             // rollIMU,pitchIMU,headingIMU,accelX,accelY,accelZ,magX,magY,magZ
  logger.include(&gps);             // lat,lon,nsats
  logger.include(&state_estimator); // x,y
  logger.include(&motor_driver);    // motorA,motorB,motorC
  logger.include(&adc);             // Current_Sense,A00,A01,A02,A03,A10,A11,A12,A13
  logger.include(&ef);              // ErrorFlagA,ErrorFlagB,ErrorFlagC
  logger.include(&button_sampler);  // Button
  logger.include(&veml6030);        // als_raw,lux
  logger.include(&anemometer);      // anemo_freq,wind_speed
  logger.include(&weatherVane);     // vane_angle,vane_voltage
  logger.init();

  printer.init();
  ef.init();
  button_sampler.init();
  imu.init();          
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  veml6030.init();      // configures VEML6030 over I2C (shared bus with IMU)
  anemometer.init();    // attaches interrupt on pin 14
  weatherVane.init();   // configures ADC for MLX90316 on pin 15

  state_estimator.init();

  printer.printMessage("Starting main loop", 10);
  loopStartTime = millis();
  printer.lastExecutionTime         = loopStartTime - LOOP_PERIOD + PRINTER_LOOP_OFFSET;
  imu.lastExecutionTime             = loopStartTime - LOOP_PERIOD + IMU_LOOP_OFFSET;
  adc.lastExecutionTime             = loopStartTime - LOOP_PERIOD + ADC_LOOP_OFFSET;
  ef.lastExecutionTime              = loopStartTime - LOOP_PERIOD + ERROR_FLAG_LOOP_OFFSET;
  button_sampler.lastExecutionTime  = loopStartTime - LOOP_PERIOD + BUTTON_LOOP_OFFSET;
  state_estimator.lastExecutionTime = loopStartTime - LOOP_PERIOD + XY_STATE_ESTIMATOR_LOOP_OFFSET;
  navLastExecutionTime              = loopStartTime - LOOP_PERIOD + SURFACE_CONTROL_LOOP_OFFSET;
  veml6030.lastExecutionTime        = loopStartTime - LOOP_PERIOD + 80;
  anemometer.lastExecutionTime      = loopStartTime - LOOP_PERIOD + 85;
  weatherVane.lastExecutionTime     = loopStartTime - LOOP_PERIOD + 90;
  logger.lastExecutionTime          = loopStartTime - LOOP_PERIOD + LOGGER_LOOP_OFFSET;
}


//////////////////////////////* Loop */////////////////////////

void loop() {
  currentTime = millis();

  if (currentTime - printer.lastExecutionTime > LOOP_PERIOD) {
    printer.lastExecutionTime = currentTime;
    printer.printValue(0, adc.printSample());
    printer.printValue(1, ef.printStates());
    printer.printValue(2, logger.printState());
    printer.printValue(3, gps.printState());
    printer.printValue(4, state_estimator.printState());
    printer.printValue(5, veml6030.printState());
    printer.printValue(6, anemometer.printState());
    printer.printValue(7, weatherVane.printState());
    printer.printValue(8, motor_driver.printState());
    printer.printValue(9, imu.printRollPitchHeading());
    printer.printValue(10, imu.printAccels());
    printer.printToSerial();
  }

  // Navigation state machine
  if (currentTime - navLastExecutionTime > LOOP_PERIOD) {
    navLastExecutionTime = currentTime;

    if (navState == NAV_IDLE) {
      motor_driver.drive(0, 0, 0);
      if (button_sampler.buttonState) {
        // Capture current heading
        targetHeading = imu.state.heading;
        navState = NAV_DRIVE;
        stateStartTime = currentTime;
        stationsCompleted = 0;
        printer.printMessage("GO! Heading: " + String(targetHeading, 1) + " deg", 10);
      }
    }

    else if (navState == NAV_DRIVE) {
      // P-controller keeps robot on target bearing
      float headingError = targetHeading - imu.state.heading;

      // Wrap error to [-180, 180] degrees
      while (headingError >  180.0) headingError -= 360.0;
      while (headingError < -180.0) headingError += 360.0;

      // Proportional correction 
      int correction = (int)(KP_HEADING * headingError);
      int uL = constrain(BASE_POWER + correction, -255, 255);
      int uR = constrain(BASE_POWER - correction, -255, 255);
      motor_driver.drive(uL, uR, BASE_POWER);

      // Transition to STATION 
      if ((unsigned long)(currentTime - stateStartTime) >= DRIVE_DURATION_MS) {
        navState = NAV_STATION;
        stateStartTime = currentTime;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("Station " + String(stationsCompleted + 1) + "/" + String(NUM_STATIONS) +
          " @ (" + String(gps.state.lat, 6) + ", " + String(gps.state.lon, 6) +
          ") sats=" + String(gps.state.num_sat), 10);
      }
    }

    else if (navState == NAV_STATION) {
      motor_driver.drive(0, 0, 0);
      // Transition after station duration elapses
      if ((unsigned long)(currentTime - stateStartTime) >= STATION_DURATION_MS) {
        stationsCompleted++;
        if (stationsCompleted >= NUM_STATIONS) {
          navState = NAV_DONE;
          printer.printMessage("All stations done!", 10);
        } else {
          navState = NAV_DRIVE;
          stateStartTime = currentTime;
          printer.printMessage("Driving to station " + String(stationsCompleted + 1), 10);
        }
      }
    }

    else {  
      motor_driver.drive(0, 0, 0);
    }
  }

  // Read ADC channels (includes BPW34 on A01 = pin 15)
  if (currentTime - adc.lastExecutionTime > LOOP_PERIOD) {
    adc.lastExecutionTime = currentTime;
    adc.updateSample();
  }


  if (currentTime - ef.lastExecutionTime > LOOP_PERIOD) {
    ef.lastExecutionTime = currentTime;
    attachInterrupt(digitalPinToInterrupt(ERROR_FLAG_A), EFA_Detected, LOW);
    attachInterrupt(digitalPinToInterrupt(ERROR_FLAG_B), EFB_Detected, LOW);
    attachInterrupt(digitalPinToInterrupt(ERROR_FLAG_C), EFC_Detected, LOW);
    delay(5);
    detachInterrupt(digitalPinToInterrupt(ERROR_FLAG_A));
    detachInterrupt(digitalPinToInterrupt(ERROR_FLAG_B));
    detachInterrupt(digitalPinToInterrupt(ERROR_FLAG_C));
    ef.updateStates(EF_States[0], EF_States[1], EF_States[2]);
    EF_States[0] = 1;
    EF_States[1] = 1;
    EF_States[2] = 1;
  }

  //Read button 
  if (currentTime - button_sampler.lastExecutionTime > LOOP_PERIOD) {
    button_sampler.lastExecutionTime = currentTime;
    button_sampler.updateState();
  }

  // Read IMU (blocking I2C)
  if (currentTime - imu.lastExecutionTime > LOOP_PERIOD) {
    imu.lastExecutionTime = currentTime;
    imu.read();
  }


  gps.read(&GPS);

  if (currentTime - state_estimator.lastExecutionTime > LOOP_PERIOD) {
    state_estimator.lastExecutionTime = currentTime;
    state_estimator.updateState(&imu.state, &gps.state);
  }

  //Read VEML6030
  if (currentTime - veml6030.lastExecutionTime > LOOP_PERIOD) {
    veml6030.lastExecutionTime = currentTime;
    veml6030.read();
  }

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


  if (currentTime - led.lastExecutionTime > LOOP_PERIOD) {
    led.lastExecutionTime = currentTime;
    led.flashLED(&gps.state);
  }


  if (currentTime - logger.lastExecutionTime > LOOP_PERIOD && logger.keepLogging) {
    logger.lastExecutionTime = currentTime;
    logger.log();
  }
}


void EFA_Detected(void){
  EF_States[0] = 0;
}

void EFB_Detected(void){
  EF_States[1] = 0;
}

void EFC_Detected(void){
  EF_States[2] = 0;
}

