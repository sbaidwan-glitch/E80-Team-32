/*
  Motor Test — cycles through each motor (A, B, C) one at a time.
  Each motor runs forward, then reverse, then stops before moving
  to the next motor. Prints status to Serial Monitor.

  Pins (V3 motherboard):
    Motor A: direction=4, speed=3
    Motor B: direction=6, speed=5
    Motor C: direction=22, speed=23
*/

// Motor pin definitions (from Pinouts.h, V3 motherboard)
#define MOTOR_A_DIR   4
#define MOTOR_A_SPD   3
#define MOTOR_B_DIR   6
#define MOTOR_B_SPD   5
#define MOTOR_C_DIR   22
#define MOTOR_C_SPD   23

#define TEST_POWER    120   // PWM power level for test (0-255)
#define RUN_TIME_MS   2000  // how long each direction runs
#define PAUSE_TIME_MS 1000  // pause between directions

const int dirPins[] = { MOTOR_A_DIR, MOTOR_B_DIR, MOTOR_C_DIR };
const int spdPins[] = { MOTOR_A_SPD, MOTOR_B_SPD, MOTOR_C_SPD };
const char* names[] = { "A (Left)", "B (Right)", "C (Center)" };

void stopAll() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(dirPins[i], LOW);
    analogWrite(spdPins[i], 0);
  }
}

void runMotor(int index, bool forward, int power) {
  digitalWrite(dirPins[index], forward ? HIGH : LOW);
  analogWrite(spdPins[index], power);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  for (int i = 0; i < 3; i++) {
    pinMode(dirPins[i], OUTPUT);
    pinMode(spdPins[i], OUTPUT);
  }
  stopAll();

  Serial.println("=== Motor Test ===");
  Serial.print("Test power: ");
  Serial.println(TEST_POWER);
  Serial.println("Each motor: forward -> pause -> reverse -> pause\n");
  Serial.println("Starting in 2 seconds...\n");
  delay(2000);
}

void loop() {
  for (int i = 0; i < 3; i++) {
    // Forward
    Serial.print("Motor ");
    Serial.print(names[i]);
    Serial.println(" — FORWARD");
    runMotor(i, true, TEST_POWER);
    delay(RUN_TIME_MS);

    // Pause
    stopAll();
    delay(PAUSE_TIME_MS);

    // Reverse
    Serial.print("Motor ");
    Serial.print(names[i]);
    Serial.println(" — REVERSE");
    runMotor(i, false, TEST_POWER);
    delay(RUN_TIME_MS);

    // Pause before next motor
    stopAll();
    Serial.println();
    delay(PAUSE_TIME_MS);
  }

  Serial.println("--- Cycle complete, restarting in 3 seconds ---\n");
  delay(3000);
}
