// MLX90316 analog weather vane on Teensy 4.0
// Assumes sensor OUT is scaled to Teensy-safe range with a voltage divider
// Potential divider: 18k top, 30k bottom.

const int vanePin = A0; // Teensy input pin

// Teensy ADC settings
const float Vref = 3.3f;
const int ADC_BITS = 12; // Teensy read resolution
const int ADC_MAX = (1 << ADC_BITS) - 1;   // 4095 for 12-bit

// Measured/calibrated voltage range at the Teensy pin.
// REPLACE!
float Vmin_meas = 0.094f;
float Vmax_meas = 3.000f;

// Measured/calibrated North Offset 
// REPLACE! Ex: if the raw angle reads 73 deg when vane points north, set northOffset = -73.
float northOffsetDeg = 0.0f;

// Set true if angle increases in the wrong direction when the vane rotates.
bool reverseDirection = false;

// Moving-average smoothing (incase of fluttering, avg last 10 samples)
const int N_SAMPLES = 10;
int sampleBuffer[N_SAMPLES];
int sampleIndex = 0;
long sampleSum = 0;
bool bufferFilled = false;

float clampf(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

// Function to wrap angle
float wrap360(float angleDeg) {
  while (angleDeg >= 360.0f) angleDeg -= 360.0f;
  while (angleDeg < 0.0f) angleDeg += 360.0f;
  return angleDeg;
}

// Return Directions (Ex. NE, E, S)
const char* headingLabel(float angleDeg) {
  angleDeg = wrap360(angleDeg);

  if (angleDeg < 22.5f || angleDeg >= 337.5f) return "N";
  if (angleDeg < 67.5f)  return "NE";
  if (angleDeg < 112.5f) return "E";
  if (angleDeg < 157.5f) return "SE";
  if (angleDeg < 202.5f) return "S";
  if (angleDeg < 247.5f) return "SW";
  if (angleDeg < 292.5f) return "W";
  return "NW";
}

// initialize first 10 readings + fill n/a data 
int readSmoothedADC() {
  int raw = analogRead(vanePin);

  // Fill buffer first time
  if (!bufferFilled) {
  sampleSum = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
      sampleBuffer[i] = raw;
      sampleSum += raw;
    }
    bufferFilled = true;
    sampleIndex = 0;
    return raw;
  }

  // Running moving average
  sampleSum -= sampleBuffer[sampleIndex];
  sampleBuffer[sampleIndex] = raw;
  sampleSum += raw;

  sampleIndex++;
  if (sampleIndex >= N_SAMPLES) sampleIndex = 0;

  return (int)(sampleSum / N_SAMPLES);
}

float adcToVoltage(int adc) {
  return (adc * Vref) / ADC_MAX;
}

// Convert Voltage to Angle
float voltageToAngle(float volts) {
  volts = clampf(volts, Vmin_meas, Vmax_meas);

  float angle = 360.0f * (volts - Vmin_meas) / (Vmax_meas - Vmin_meas);

  if (reverseDirection) {
    angle = 360.0f - angle;
  }

  angle = angle + northOffsetDeg;
  return wrap360(angle);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(ADC_BITS); 

  delay(500);
  Serial.println("MLX90316 Analog Weather Vane");
  Serial.println("Time,ADC,Voltage,Angle,Heading");
}

void loop() {
  int adc = readSmoothedADC();
  float volts = adcToVoltage(adc);
  float angleDeg = voltageToAngle(volts);
  const char* heading = headingLabel(angleDeg);

  // Print in CSV format
  Serial.print(millis()); // time
  Serial.print(",");
  Serial.print(adc); // raw teensy output
  Serial.print(",");
  Serial.print(volts, 4); // Voltage (use to calibrate)
  Serial.print(",");
  Serial.print(angleDeg, 2); // Angle
  Serial.print(",");
  Serial.println(heading); // Angle Heading (NE, E, ...)

  delay(200); // 1 print every 200ms = 5 Hz (change to slower (500) or faster?)
}


  // Serial.print("ADC: "); // raw integer from Teensy ADC 
  // Serial.print(adc);
  // Serial.print("   V: "); // converted voltage value, 3 decimals
  // Serial.print(volts, 3); 
  // Serial.print("   Angle(deg): "); // Angle from mapping equation
  // Serial.print(angleDeg, 1);
  // Serial.print("   Heading: "); // Angle Headings
  // Serial.println(heading);
