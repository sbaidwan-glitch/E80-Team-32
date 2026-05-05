/*
  Dana Point - Open Loop LONG (backup for full-mission day)
  E80 Team 32

  Backup plan if the closed-loop heading hold is unreliable on the day of
  Dana Point. Same mission geometry as DanaPointClosed (5 stations, 2 min
  of drive + 5 min of station per waypoint), but with zero navigation
  feedback: motors run at a constant forward command during each leg and
  go fully off during stations.

  No heading hold, no button, no reverse. If the boat drifts off course
  during a leg, it drifts -- easier to post-process a straight-ish dead-
  reckoning track than to trust a mag that was swamped by motor EMI.

  Flow (total ~35.5 min wall-clock):
    1. 30 s countdown (place boat in water, point bow toward waypoints).
    2. Leg 1: 2 min of constant motor thrust. All sensors continue
       logging at 10 Hz.
    3. Station 1: motors off, 5 min of clean sensor collection.
    4. Legs 2..5 and stations 2..5 repeat the same pattern.
    5. Halt permanently; sensors still log until power-off.

  WARNING: Motors run continuously during each 2 min leg. Keep hands and
  cables clear of the props.

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
#include <ADCSampler.h>
#include <ErrorFlagSampler.h>
#include <MotorDriver.h>
#include <Logger.h>
#include <Printer.h>
#define UartSerial Serial1
#include <GPSLockLED.h>

#include <SensorVEML6030.h>
#include <SensorAnemometer.h>
#include <SensorWeatherVane.h>

// ---- Mission geometry (matches DanaPointClosed) ----
#define NUM_STATIONS         6
#define DRIVE_DURATION_MS    30000UL   // 1 min per leg (wall-clock)
#define STATION_DURATION_MS  30000UL   // 1 min per station

// ---- Motor powers ----
// H-bridge wiring has V+ and GND swapped, so NEGATIVE commands = FORWARD.
// Motor labels are swapped too: "Motor A" is the RIGHT thruster, C is LEFT.
// Magnitudes tuned in open-loop bench testing to make the boat go straight.
#define MOTOR_A_POWER        -200    // right motor
#define MOTOR_B_POWER        -5      // small trim
#define MOTOR_C_POWER        -255    // left motor

// ---- Motor-pause-for-mag cycle ----
// Even in open-loop, the LSM303AGR is swamped by motor EMI when motors
// run continuously. Cycle motors off briefly so the logged heading has
// usable values during legs. No controller -- just clean sampling.
#define THRUST_MS            2000UL   // motors on for 2s
#define PAUSE_MS             1000UL   // motors off for 1s
#define HEADING_SAMPLE_MS    50UL     // sample every 50ms during pause

// ---- Navigation states ----
#define OL_DRIVE     0
#define OL_STATION   1
#define OL_DONE      2

/////////////////////////* Global Variables *////////////////////////

MotorDriver motor_driver;
SensorGPS gps;
Adafruit_GPS GPS(&UartSerial);
ADCSampler adc;
ErrorFlagSampler ef;
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

int navState = OL_DRIVE;
unsigned long stateStartTime = 0;
int stationsCompleted = 0;
unsigned long navLastExecutionTime;

// Motor-pause cycle (within OL_DRIVE)
bool drivePaused = false;
unsigned long driveCycleStart = 0;
unsigned long lastHeadingSampleTime = 0;
int headingSampleCount = 0;

// sin/cos averaging across the 360->0 wrap, like DanaPointClosed
float headingSinSum = 0.0f;
float headingCosSum = 0.0f;
float lastCleanHeading = 0.0f;

////////////////////////* Setup *////////////////////////////////

void setup() {
  logger.include(&imu);             // rollIMU,pitchIMU,headingIMU,accelX/Y/Z,magX/Y/Z
  logger.include(&gps);             // lat,lon,nsats
  logger.include(&motor_driver);    // motorA,motorB,motorC
  logger.include(&adc);             // Current_Sense,A00..A13 (A02 = BPW34)
  logger.include(&ef);              // ErrorFlagA,B,C
  logger.include(&veml6030);        // als_raw,lux
  logger.include(&anemometer);      // anemo_freq,wind_speed,anemo_pulses
  logger.include(&weatherVane);     // vane_angle,vane_voltage
  logger.init();

  printer.init();
  ef.init();
  imu.init();
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  veml6030.init();
  anemometer.init();
  weatherVane.init();

  printer.printMessage("Dana Point OPEN-LONG - 30s countdown", 10);
  for (int i = 30; i > 0; i--) {
    Serial.print("Starting in ");
    Serial.print(i);
    Serial.println("s...");
    delay(1000);
  }
  Serial.println("GO!");

  anemometer.reset();

  loopStartTime = millis();
  stateStartTime = loopStartTime;
  driveCycleStart = loopStartTime;
  drivePaused = false;
  navState = OL_DRIVE;
  stationsCompleted = 0;

  printer.lastExecutionTime    = loopStartTime - LOOP_PERIOD + PRINTER_LOOP_OFFSET;
  imu.lastExecutionTime        = loopStartTime - LOOP_PERIOD + IMU_LOOP_OFFSET;
  adc.lastExecutionTime        = loopStartTime - LOOP_PERIOD + ADC_LOOP_OFFSET;
  ef.lastExecutionTime         = loopStartTime - LOOP_PERIOD + ERROR_FLAG_LOOP_OFFSET;
  navLastExecutionTime         = loopStartTime - LOOP_PERIOD + SURFACE_CONTROL_LOOP_OFFSET;
  veml6030.lastExecutionTime   = loopStartTime - LOOP_PERIOD + 80;
  anemometer.lastExecutionTime = loopStartTime - LOOP_PERIOD + 85;
  weatherVane.lastExecutionTime= loopStartTime - LOOP_PERIOD + 90;
  logger.lastExecutionTime     = loopStartTime - LOOP_PERIOD + LOGGER_LOOP_OFFSET;

  // Kick off the first leg
  motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);
}

//////////////////////////////* Loop */////////////////////////

void loop() {
  currentTime = millis();

  if (currentTime - printer.lastExecutionTime > LOOP_PERIOD) {
    printer.lastExecutionTime = currentTime;
    printer.printValue(0, ef.printStates());
    printer.printValue(1, logger.printState());
    printer.printValue(2, gps.printState());
    printer.printValue(3, veml6030.printState());
    printer.printValue(4, anemometer.printState());
    printer.printValue(5, weatherVane.printState());
    printer.printValue(6, motor_driver.printState());
    printer.printValue(7, imu.printRollPitchHeading());
    printer.printValue(8, "navState: " + String(navState) +
                          " paused: " + String(drivePaused) +
                          " lastClean: " + String(lastCleanHeading, 1) +
                          " stn: " + String(stationsCompleted) +
                          "/" + String(NUM_STATIONS));
    // BPW34 photodiode = ADC sample[3] = pin 16 (A02). 12-bit → 3.3V.
    printer.printValue(9, "BPW34: adc=" + String(adc.sample[3]) +
                           " V=" + String(adc.sample[3] * 3.3f / 4095.0f, 3));
    printer.printToSerial();
  }

  // ----- Navigation state machine (no heading hold) -----
  if (currentTime - navLastExecutionTime > LOOP_PERIOD) {
    navLastExecutionTime = currentTime;

    if (navState == OL_DRIVE) {
      unsigned long cycleElapsed = (unsigned long)(currentTime - driveCycleStart);

      if (!drivePaused) {
        // -- Thrust phase: constant motor commands, no controller --
        motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);

        if (cycleElapsed >= THRUST_MS) {
          // Enter pause: cut motors, start mag-sample window
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
        // -- Sample phase: motors off, accumulate clean mag readings --
        motor_driver.drive(0, 0, 0);

        if ((unsigned long)(currentTime - lastHeadingSampleTime) >= HEADING_SAMPLE_MS) {
          float hRad = imu.state.heading * (float)M_PI / 180.0f;
          headingSinSum += sinf(hRad);
          headingCosSum += cosf(hRad);
          headingSampleCount++;
          lastHeadingSampleTime = currentTime;
        }

        if (cycleElapsed >= PAUSE_MS) {
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

      if ((unsigned long)(currentTime - stateStartTime) >= DRIVE_DURATION_MS) {
        navState = OL_STATION;
        stateStartTime = currentTime;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("Station " + String(stationsCompleted + 1) +
          "/" + String(NUM_STATIONS) +
          " @ (" + String(gps.state.lat, 6) + "," + String(gps.state.lon, 6) +
          ") sats=" + String(gps.state.num_sat), 10);
      }
    }

    else if (navState == OL_STATION) {
      motor_driver.drive(0, 0, 0);

      if ((unsigned long)(currentTime - stateStartTime) >= STATION_DURATION_MS) {
        stationsCompleted++;
        if (stationsCompleted >= NUM_STATIONS) {
          navState = OL_DONE;
          printer.printMessage("All stations done!", 10);
        } else {
          navState = OL_DRIVE;
          stateStartTime = currentTime;
          driveCycleStart = currentTime;
          drivePaused = false;
          motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);
          printer.printMessage("Driving to station " +
                                String(stationsCompleted + 1), 10);
        }
      }
    }

    else {
      motor_driver.drive(0, 0, 0);
    }
  }

  // ----- Sensor / logging ticks (run continuously regardless of state) -----

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

  if (currentTime - imu.lastExecutionTime > LOOP_PERIOD) {
    imu.lastExecutionTime = currentTime;
    imu.read();
  }

  gps.read(&GPS);

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
