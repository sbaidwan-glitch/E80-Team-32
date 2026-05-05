/*
  Dana Point - Closed Loop Version
  E80 Team 32

  Heading-hold navigation with motor-pause-for-magnetometer sampling.
  Motors A + C provide straight-line thrust; Motor B is off by default
  (adjust MOTOR_B_POWER at top if day-of tuning shows B is needed).

  Motor EMI corrupts the LSM303AGR reading while motors run, so the
  controller periodically cuts all motors for PAUSE_DURATION_MS to
  capture a clean heading sample, averages N samples, and uses that
  as feedback for the P-controller during the next thrust window.

  Flow:
    1. 30-second countdown (no button needed). Target heading is captured
       at the end of the countdown while motors are off, so the initial
       magnetometer reading is clean.
    2. Drive toward heading for DRIVE_DURATION_MS (wall-clock),
       cycling THRUST_MS on / PAUSE_MS off for clean mag reads.
    3. Stop at station for STATION_DURATION_MS, log data.
    4. Resume driving on original heading to next station.
    5. Repeat NUM_STATIONS times, then halt.

  WARNING: Motors will spin after the 30s countdown. Keep hands and
  cables clear of propellers.

  Pin map (MTB J1):
    Pin 14 (A00) - AH9246 anemometer digital pulses
    Pin 15 (A01) - MLX90316 weather vane analog out
    Pin 16 (A02) - BPW34 photodiode + TIA analog out   (logged by ADCSampler)
    Pin 18 (SDA) - VEML6030 + LSM303AGR I2C data
    Pin 19 (SCL) - VEML6030 + LSM303AGR I2C clock
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

#include <SensorVEML6030.h>
#include <SensorAnemometer.h>
#include <SensorWeatherVane.h>

// ---- Mission geometry ----
#define NUM_STATIONS         5
#define DRIVE_DURATION_MS    60000UL   // 1 min per leg (wall-clock)
#define STATION_DURATION_MS  60000UL   // 1 min per station

// ---- Motor straight-drive config ----
// H-bridge wiring has V+ and GND swapped, so NEGATIVE commands = FORWARD
// for all three motors. Motor labels on the hardware are also swapped:
// "Motor A" is physically the RIGHT thruster (-255 on A spins the boat
// counterclockwise because the right motor is driving). The asymmetric
// magnitudes below (A=-200, C=-255, B=-5) are the empirical values that
// made the boat go straight in open-loop bench testing.
//
// The P-controller steers by differentially easing thrust: +correction
// slows A (boat turns right), -correction slows C (boat turns left).
// uA and uC are CLAMPED to [-255, 0] so the controller can only REDUCE
// forward thrust -- it can never flip a motor into reverse.
#define BASE_A               -200    // right motor (slightly weaker magnitude)
#define BASE_C               -255    // left motor  (full thrust)
#define MOTOR_B_POWER        -5      // small trim, always on

// ---- Heading P-controller ----
// KP bumped from 0.5 -> 2.0 after Dana Point test 1 veered in a left
// semicircle: 0.5 * (<20 deg error) was below MOTOR_DEADZONE=34 so the
// correction got swallowed entirely. If this overcorrects / oscillates,
// drop to 1.0. If it turns the wrong way, flip CORRECTION_SIGN to -1.
#define KP_HEADING           2.0f

// Flip to -1 if the correction drives the boat the wrong way during
// Phake Lake testing (depends on motor mounting geometry).
#define CORRECTION_SIGN      1

// ---- Motor-pause-for-mag cycle ----
// THRUST_MS reduced from 5000 -> 2000 and PAUSE_MS raised from 500 -> 1000
// so lastCleanHeading updates every ~3 s (was 5.5 s) and motor EMI has
// longer to decay before we sample the magnetometer.
#define THRUST_MS            2000UL   // motors on for 2s
#define PAUSE_MS             1000UL   // motors off for 1s
#define HEADING_SAMPLE_MS    50UL     // take a mag sample every 50ms during pause
                                      // -> ~20 samples averaged per pause

// ---- Navigation states ----
#define NAV_IDLE     0
#define NAV_DRIVE    1
#define NAV_STATION  2
#define NAV_DONE     3

// Minimal DataSource that logs the P-controller's internal state each
// tick: the commanded heading, the motors-off mag-averaged reading the
// controller is actually feeding back on, and the wrap-corrected error.
// Lets post-run analysis distinguish "mag was clean but controller did
// the wrong thing" from "mag was still noisy during pauses".
class NavLogger : public DataSource {
public:
  float target;
  float clean;
  float error;
  NavLogger() : DataSource("tgt_hdg,clean_hdg,hdg_err", "float,float,float") {
    target = 0.0f; clean = 0.0f; error = 0.0f;
  }
  size_t writeDataBytes(unsigned char * buffer, size_t idx) override {
    float * slot = (float *) &buffer[idx];
    slot[0] = target;
    slot[1] = clean;
    slot[2] = error;
    return idx + 3 * sizeof(float);
  }
};

/////////////////////////* Global Variables *////////////////////////

NavLogger nav_logger;
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

SensorVEML6030 veml6030;
SensorAnemometer anemometer;
SensorWeatherVane weatherVane;

unsigned long loopStartTime;
unsigned long currentTime;
volatile bool EF_States[NUM_FLAGS] = {1, 1, 1};

// Navigation state
int navState = NAV_IDLE;
float targetHeading = 0.0f;         // desired heading (captured at button press)
float lastCleanHeading = 0.0f;      // most recent motors-off heading sample
unsigned long stateStartTime = 0;
int stationsCompleted = 0;
unsigned long navLastExecutionTime;

// Motor-pause cycle (within NAV_DRIVE)
bool drivePaused = false;
unsigned long driveCycleStart = 0;
unsigned long lastHeadingSampleTime = 0;
float headingSampleSum = 0.0f;
int headingSampleCount = 0;

// Heading averaging during pause. Magnetometer readings wrap at 360,
// so we average sin/cos components instead of the raw angle to avoid
// the 359 -> 1 deg transition ruining the mean.
float headingSinSum = 0.0f;
float headingCosSum = 0.0f;

////////////////////////* Setup *////////////////////////////////

void setup() {
  logger.include(&imu);             // rollIMU,pitchIMU,headingIMU,accelX/Y/Z,magX/Y/Z
  logger.include(&gps);             // lat,lon,nsats
  logger.include(&state_estimator); // x,y
  logger.include(&motor_driver);    // motorA,motorB,motorC
  logger.include(&adc);             // Current_Sense,A00,A01,A02,A03,A10..A13
  logger.include(&ef);              // ErrorFlagA,B,C
  logger.include(&button_sampler);  // Button
  logger.include(&veml6030);        // als_raw,lux
  logger.include(&anemometer);      // anemo_freq,wind_speed,anemo_pulses
  logger.include(&weatherVane);     // vane_angle,vane_voltage
  logger.include(&nav_logger);      // tgt_hdg,clean_hdg,hdg_err
  logger.init();

  printer.init();
  ef.init();
  button_sampler.init();
  imu.init();
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  veml6030.init();
  anemometer.init();    // interrupt on pin 14
  weatherVane.init();   // analog read on pin 15
  state_estimator.init();

  printer.printMessage("Dana Point CLOSED-LOOP - 30s countdown", 10);
  for (int i = 30; i > 0; i--) {
    Serial.print("Starting in ");
    Serial.print(i);
    Serial.println("s...");
    delay(1000);
  }
  Serial.println("GO!");

  // Motors are off during countdown, so imu.state.heading after a few
  // fresh reads is a clean target heading (no EMI). Pump the IMU a
  // handful of times before capturing so the reading has settled.
  for (int i = 0; i < 5; i++) {
    imu.read();
    delay(20);
  }
  targetHeading = imu.state.heading;
  lastCleanHeading = targetHeading;
  navState = NAV_DRIVE;
  stationsCompleted = 0;

  // Clear any anemometer pulses that accumulated during the 30s countdown
  // so the first wind sample is measured from the mission start, not boot.
  anemometer.reset();

  loopStartTime = millis();
  stateStartTime  = loopStartTime;
  driveCycleStart = loopStartTime;
  drivePaused     = false;

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
    printer.printValue(0, ef.printStates());
    printer.printValue(1, logger.printState());
    printer.printValue(2, gps.printState());
    printer.printValue(3, state_estimator.printState());
    printer.printValue(4, veml6030.printState());
    printer.printValue(5, anemometer.printState());
    printer.printValue(6, weatherVane.printState());
    printer.printValue(7, motor_driver.printState());
    printer.printValue(8, imu.printRollPitchHeading());
    printer.printValue(9, "navState: " + String(navState) +
                            " paused: " + String(drivePaused) +
                            " tgt: " + String(targetHeading, 1) +
                            " last: " + String(lastCleanHeading, 1));
    // BPW34 photodiode = ADC sample[3] = pin 16 (A02). 12-bit → 3.3V.
    printer.printValue(10, "BPW34: adc=" + String(adc.sample[3]) +
                            " V=" + String(adc.sample[3] * 3.3f / 4095.0f, 3));
    printer.printToSerial();
  }

  // ----- Navigation state machine -----
  if (currentTime - navLastExecutionTime > LOOP_PERIOD) {
    navLastExecutionTime = currentTime;

    if (navState == NAV_DRIVE) {
      unsigned long cycleElapsed = (unsigned long)(currentTime - driveCycleStart);

      if (!drivePaused) {
        // -- Thrust phase: drive using last clean heading as feedback --
        float headingError = targetHeading - lastCleanHeading;
        while (headingError >  180.0f) headingError -= 360.0f;
        while (headingError < -180.0f) headingError += 360.0f;

        nav_logger.target = targetHeading;
        nav_logger.clean  = lastCleanHeading;
        nav_logger.error  = headingError;

        int correction = (int)(CORRECTION_SIGN * KP_HEADING * headingError);
        // Clamp to [-255, 0]: correction can only EASE thrust, never
        // reverse a motor. This keeps the boat always going forward.
        int uA = constrain(BASE_A + correction, -255, 0);
        int uC = constrain(BASE_C - correction, -255, 0);
        motor_driver.drive(uA, MOTOR_B_POWER, uC);

        if (cycleElapsed >= THRUST_MS) {
          // Enter pause: cut motors, start heading averaging window
          drivePaused = true;
          motor_driver.drive(0, 0, 0);
          driveCycleStart = currentTime;
          lastHeadingSampleTime = currentTime;
          headingSinSum = 0.0f;
          headingCosSum = 0.0f;
          headingSampleCount = 0;
        }
      }
      else {
        // -- Sample phase: motors off, accumulate mag readings --
        motor_driver.drive(0, 0, 0);

        if ((unsigned long)(currentTime - lastHeadingSampleTime) >= HEADING_SAMPLE_MS) {
          float hRad = imu.state.heading * (float)M_PI / 180.0f;
          headingSinSum += sinf(hRad);
          headingCosSum += cosf(hRad);
          headingSampleCount++;
          lastHeadingSampleTime = currentTime;
        }

        if (cycleElapsed >= PAUSE_MS) {
          // End pause: compute averaged heading, resume thrusting
          if (headingSampleCount > 0) {
            float avgRad = atan2f(headingSinSum, headingCosSum);
            float avgDeg = avgRad * 180.0f / (float)M_PI;
            while (avgDeg < 0.0f)    avgDeg += 360.0f;
            while (avgDeg >= 360.0f) avgDeg -= 360.0f;
            lastCleanHeading = avgDeg;
          }
          drivePaused = false;
          driveCycleStart = currentTime;
        }
      }

      // Leg done? (wall-clock, including pauses)
      if ((unsigned long)(currentTime - stateStartTime) >= DRIVE_DURATION_MS) {
        navState = NAV_STATION;
        stateStartTime = currentTime;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("Station " + String(stationsCompleted + 1) +
          "/" + String(NUM_STATIONS) +
          " @ (" + String(gps.state.lat, 6) + "," + String(gps.state.lon, 6) +
          ") sats=" + String(gps.state.num_sat), 10);
      }
    }

    else if (navState == NAV_STATION) {
      motor_driver.drive(0, 0, 0);
      if ((unsigned long)(currentTime - stateStartTime) >= STATION_DURATION_MS) {
        stationsCompleted++;
        if (stationsCompleted >= NUM_STATIONS) {
          navState = NAV_DONE;
          printer.printMessage("All stations done!", 10);
        } else {
          // Re-sample a clean heading before resuming (motors have been off
          // for STATION_DURATION_MS, so imu.state.heading is trustworthy).
          lastCleanHeading = imu.state.heading;
          navState = NAV_DRIVE;
          stateStartTime = currentTime;
          driveCycleStart = currentTime;
          drivePaused = false;
          printer.printMessage("Driving to station " +
                                String(stationsCompleted + 1), 10);
        }
      }
    }

    else {
      motor_driver.drive(0, 0, 0);
    }
  }

  // ----- Sensor / logging ticks -----

  if (currentTime - adc.lastExecutionTime > LOOP_PERIOD) {
    adc.lastExecutionTime = currentTime;
    adc.updateSample();
  }

  if (currentTime - ef.lastExecutionTime > LOOP_PERIOD) {
    ef.lastExecutionTime = currentTime;
    // Poll directly. INPUT_PULLUP in ErrorFlagSampler::init() makes pins
    // read HIGH when no fault; driver chip pulls LOW to assert a fault.
    ef.updateStates(digitalRead(ERROR_FLAG_A),
                    digitalRead(ERROR_FLAG_B),
                    digitalRead(ERROR_FLAG_C));
  }

  if (currentTime - button_sampler.lastExecutionTime > LOOP_PERIOD) {
    button_sampler.lastExecutionTime = currentTime;
    button_sampler.updateState();
  }

  if (currentTime - imu.lastExecutionTime > LOOP_PERIOD) {
    imu.lastExecutionTime = currentTime;
    imu.read();
  }

  gps.read(&GPS);

  if (currentTime - state_estimator.lastExecutionTime > LOOP_PERIOD) {
    state_estimator.lastExecutionTime = currentTime;
    state_estimator.updateState(&imu.state, &gps.state);
  }

  if (currentTime - veml6030.lastExecutionTime > LOOP_PERIOD) {
    veml6030.lastExecutionTime = currentTime;
    veml6030.read();
  }

  if (currentTime - anemometer.lastExecutionTime > LOOP_PERIOD) {
    anemometer.lastExecutionTime = currentTime;
    anemometer.read();
  }

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

void EFA_Detected(void) { EF_States[0] = 0; }
void EFB_Detected(void) { EF_States[1] = 0; }
void EFC_Detected(void) { EF_States[2] = 0; }
