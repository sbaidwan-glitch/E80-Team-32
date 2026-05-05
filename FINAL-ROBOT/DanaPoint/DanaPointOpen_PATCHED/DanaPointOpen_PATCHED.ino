/*
  Dana Point - Open Loop (Backup) Version
  E80 Team 32

  Dead-reckoning backup in case the magnetometer is unreliable day-of.
  No heading hold, no button press. Starts on a 30-second countdown.

  Motor A and Motor C drive forward; Motor B is off (matches the
  empirically-straight config). Thrust duty-cycles (5s on / 10s off)
  so each pause window gives clean sensor readings without motor EMI.

  After DRIVE_DURATION_MS of accumulated *actual* driving time, the
  motors reverse for the same accumulated time, then the robot halts.
  This gives a there-and-back pattern with no navigation feedback.

  Same sensor + logging set as the closed-loop sketch; the only
  differences are: no button, no heading control, no stations.

  WARNING: Motors will spin at full power after the 30s countdown.
  Keep hands and cables clear of propellers.

  Pin map (MTB J1) - same as closed-loop:
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

// ---- Drive budget (real thrust time, not wall-clock) ----
#define DRIVE_DURATION_MS   60000UL   // 1 min forward, 1 min reverse

// ---- Motor powers ----
// A + C on, B off => straight (matches bench observation). If day-of
// tuning shows the boat veers, adjust MOTOR_A_POWER / MOTOR_C_POWER.
#define MOTOR_A_POWER       200
#define MOTOR_B_POWER       0
#define MOTOR_C_POWER       200

// ---- Drive / pause duty cycle (for clean sensor reads) ----
#define DRIVE_ON_MS         5000UL    // motors on for 5s
#define DRIVE_PAUSE_MS      10000UL   // motors off for 10s

// ---- Countdown before motors engage ----
#define COUNTDOWN_SEC       30

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

bool reversing = false;
bool done = false;
unsigned long totalDriveTime = 0;     // accumulated real driving time this leg
unsigned long cycleStartTime = 0;     // when current on/off cycle began
unsigned long lastDriveCheck = 0;     // last time we incremented totalDriveTime
bool motorsPaused = false;

////////////////////////* Setup *////////////////////////////////

void setup() {
  logger.include(&imu);
  logger.include(&gps);
  logger.include(&motor_driver);
  logger.include(&adc);               // picks up BPW34 on A03 (pin 17)
  logger.include(&ef);
  logger.include(&veml6030);
  logger.include(&anemometer);
  logger.include(&weatherVane);
  logger.init();

  printer.init();
  ef.init();
  imu.init();
  UartSerial.begin(9600);
  gps.init(&GPS);
  motor_driver.init();
  led.init();
  veml6030.init();
  anemometer.init();    // interrupt on pin 14
  weatherVane.init();   // analog read on pin 15

  printer.printMessage("Dana Point OPEN-LOOP - 30s countdown", 10);
  for (int i = COUNTDOWN_SEC; i > 0; i--) {
    Serial.print("Starting in ");
    Serial.print(i);
    Serial.println("s...");
    delay(1000);
  }
  Serial.println("GO - FORWARD!");

  // Clear any pre-launch pulses collected during the 30 s countdown.
  anemometer.reset();

  loopStartTime = millis();
  printer.lastExecutionTime    = loopStartTime - LOOP_PERIOD + PRINTER_LOOP_OFFSET;
  imu.lastExecutionTime        = loopStartTime - LOOP_PERIOD + IMU_LOOP_OFFSET;
  adc.lastExecutionTime        = loopStartTime - LOOP_PERIOD + ADC_LOOP_OFFSET;
  ef.lastExecutionTime         = loopStartTime - LOOP_PERIOD + ERROR_FLAG_LOOP_OFFSET;
  veml6030.lastExecutionTime   = loopStartTime - LOOP_PERIOD + 80;
  anemometer.lastExecutionTime = loopStartTime - LOOP_PERIOD + 85;
  weatherVane.lastExecutionTime= loopStartTime - LOOP_PERIOD + 90;
  logger.lastExecutionTime     = loopStartTime - LOOP_PERIOD + LOGGER_LOOP_OFFSET;

  cycleStartTime = millis();
  lastDriveCheck = millis();
  totalDriveTime = 0;
  motorsPaused = false;

  motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);
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
    printer.printValue(4, veml6030.printState());
    printer.printValue(5, anemometer.printState());
    printer.printValue(6, weatherVane.printState());
    printer.printValue(7, motor_driver.printState());
    printer.printValue(8, imu.printRollPitchHeading());
    printer.printValue(9, String("drive_s=") + String(totalDriveTime / 1000) +
                          " paused=" + String(motorsPaused) +
                          " rev=" + String(reversing));
    printer.printToSerial();
  }

  // ----- Motor duty cycle with real-drive-time budget -----
  if (!done) {
    int dirA = reversing ? -MOTOR_A_POWER : MOTOR_A_POWER;
    int dirB = reversing ? -MOTOR_B_POWER : MOTOR_B_POWER;
    int dirC = reversing ? -MOTOR_C_POWER : MOTOR_C_POWER;

    unsigned long cycleElapsed = (unsigned long)(currentTime - cycleStartTime);

    if (!motorsPaused) {
      motor_driver.drive(dirA, dirB, dirC);
      totalDriveTime += (unsigned long)(currentTime - lastDriveCheck);
      lastDriveCheck = currentTime;

      if (cycleElapsed >= DRIVE_ON_MS) {
        motorsPaused = true;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("PAUSE - collecting sensor data (10s)", 10);
      }
    } else {
      motor_driver.drive(0, 0, 0);

      if (cycleElapsed >= DRIVE_PAUSE_MS) {
        motorsPaused = false;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;
        motor_driver.drive(dirA, dirB, dirC);
        printer.printMessage(reversing ? "DRIVING (reverse)" : "DRIVING (forward)", 10);
      }
    }

    if (totalDriveTime >= DRIVE_DURATION_MS) {
      if (!reversing) {
        reversing = true;
        totalDriveTime = 0;
        cycleStartTime = currentTime;
        lastDriveCheck = currentTime;
        motorsPaused = false;
        motor_driver.drive(-MOTOR_A_POWER, -MOTOR_B_POWER, -MOTOR_C_POWER);
        printer.printMessage("REVERSING - 1 min reverse", 10);
      } else {
        done = true;
        motor_driver.drive(0, 0, 0);
        printer.printMessage("DONE - forward + reverse complete", 10);
      }
    }
  } else {
    motor_driver.drive(0, 0, 0);
  }

  // ----- Sensor / logging ticks -----

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
