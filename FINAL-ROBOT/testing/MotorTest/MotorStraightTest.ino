/*
  Motor Straight-Line Calibration Test
  E80 Team 32

  Drives all three motors for 5 seconds, pauses for 5 seconds, repeats.
  Use this to tune motor powers so the boat drives straight.
  Starts after a 15-second countdown (no button press).

  Adjust these three values until the boat goes straight:
    MOTOR_A_POWER  (line 24) — left motor
    MOTOR_B_POWER  (line 25) — right motor
    MOTOR_C_POWER  (line 26) — forward thruster

  Tips:
    - If boat veers LEFT, reduce MOTOR_A or increase MOTOR_B
    - If boat veers RIGHT, reduce MOTOR_B or increase MOTOR_A
    - Use negative values to reverse a motor direction
    - Runs forever until you power off
*/

#include <Arduino.h>
#include <Pinouts.h>
#include <MotorDriver.h>

// ---- TUNE THESE ----
#define MOTOR_A_POWER       150     // left motor (-255 to 255)
#define MOTOR_B_POWER       150     // right motor (-255 to 255)
#define MOTOR_C_POWER       150     // forward thruster (-255 to 255)

// Drive/pause timing
#define DRIVE_MS            5000    // drive for 5 seconds
#define PAUSE_MS            5000    // pause for 5 seconds

// Countdown before starting
#define COUNTDOWN_SEC       15

MotorDriver motor_driver;

unsigned long cycleStartTime = 0;
bool driving = false;
int cycleCount = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  motor_driver.init();

  Serial.println("=== Motor Straight-Line Test ===");
  Serial.print("A="); Serial.print(MOTOR_A_POWER);
  Serial.print("  B="); Serial.print(MOTOR_B_POWER);
  Serial.print("  C="); Serial.println(MOTOR_C_POWER);
  Serial.println();

  for (int i = COUNTDOWN_SEC; i > 0; i--) {
    Serial.print("Starting in ");
    Serial.print(i);
    Serial.println("s...");
    delay(1000);
  }

  Serial.println("GO!");
  Serial.println("cycle,state,motorA,motorB,motorC");

  // Start first drive cycle
  driving = true;
  cycleCount = 1;
  cycleStartTime = millis();
  motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);

  Serial.print(cycleCount); Serial.print(",DRIVE,");
  Serial.print(MOTOR_A_POWER); Serial.print(",");
  Serial.print(MOTOR_B_POWER); Serial.print(",");
  Serial.println(MOTOR_C_POWER);
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - cycleStartTime;

  if (driving) {
    motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);

    if (elapsed >= DRIVE_MS) {
      // Switch to pause
      driving = false;
      cycleStartTime = now;
      motor_driver.drive(0, 0, 0);

      Serial.print(cycleCount); Serial.println(",PAUSE,0,0,0");
    }
  } else {
    motor_driver.drive(0, 0, 0);

    if (elapsed >= PAUSE_MS) {
      // Switch to drive
      driving = true;
      cycleCount++;
      cycleStartTime = now;
      motor_driver.drive(MOTOR_A_POWER, MOTOR_B_POWER, MOTOR_C_POWER);

      Serial.print(cycleCount); Serial.print(",DRIVE,");
      Serial.print(MOTOR_A_POWER); Serial.print(",");
      Serial.print(MOTOR_B_POWER); Serial.print(",");
      Serial.println(MOTOR_C_POWER);
    }
  }
}
