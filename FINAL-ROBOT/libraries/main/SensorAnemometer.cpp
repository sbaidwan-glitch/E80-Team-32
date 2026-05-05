#include "SensorAnemometer.h"
#include "Printer.h"

extern Printer printer;

static volatile unsigned long g_pulseCount = 0;
static volatile unsigned long g_lastPulseMicros = 0;

// Interrupt: counts magnet passes with debounce.
static void anemometerISR(void) {
  unsigned long nowUs = micros();
  if (nowUs - g_lastPulseMicros >= MIN_PULSE_SPACING_US) {
    g_pulseCount++;
    g_lastPulseMicros = nowUs;
  }
}

SensorAnemometer::SensorAnemometer(void)
  : DataSource("anemo_freq,wind_speed,anemo_pulses", "float,float,int")
{
  frequency = 0.0f;
  windSpeed = 0.0f;
  lastPulseCount = 0;
  lastElapsedMs = 0;
  lastReadTime = 0;
  rpmIdx = 0;
  rpmFilled = false;
  clearAverage();
}

void SensorAnemometer::clearAverage(void) {
  rpmIdx = 0;
  rpmFilled = false;
  for (int i = 0; i < ANEMO_N_AVG; i++) {
    rpmHist[i] = 0.0f;
  }
}

void SensorAnemometer::init(void) {
  // AH9246 is open-drain — needs a pull-up for a stable idle HIGH.
  // Without this, a floating pin 14 noise-triggers FALLING thousands of
  // times/sec and can saturate the NVIC / starve SysTick.
  pinMode(ANEMOMETER_PIN, INPUT_PULLUP);
  // Sensor goes LOW when magnet passes -> FALLING edge.
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, FALLING);
  reset();
  Serial.println("Anemometer init OK (pin " + String(ANEMOMETER_PIN) + ")");
}

void SensorAnemometer::reset(void) {
  noInterrupts();
  g_pulseCount = 0;
  g_lastPulseMicros = 0;
  interrupts();

  frequency = 0.0f;
  windSpeed = 0.0f;
  lastPulseCount = 0;
  lastElapsedMs = 0;
  lastReadTime = millis();
  clearAverage();
}

float SensorAnemometer::movingAverageRPM(float newVal) {
  rpmHist[rpmIdx] = newVal;
  rpmIdx++;

  if (rpmIdx >= ANEMO_N_AVG) {
    rpmIdx = 0;
    rpmFilled = true;
  }

  int n = rpmFilled ? ANEMO_N_AVG : rpmIdx;
  if (n <= 0) return newVal;

  float sum = 0.0f;
  for (int i = 0; i < n; i++) {
    sum += rpmHist[i];
  }
  return sum / n;
}

void SensorAnemometer::read(void) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastReadTime;

  // Only compute a new wind estimate on a longer window.
  if (elapsed < ANEMO_SAMPLE_PERIOD_MS) {
    return;
  }

  noInterrupts();
  unsigned long count = g_pulseCount;
  g_pulseCount = 0;
  interrupts();

  lastPulseCount = count;
  lastElapsedMs = elapsed;

  // Frequency in Hz = pulses / elapsed seconds.
  frequency = (float)count / ((float)elapsed / 1000.0f);

  // Convert to RPM (assumes 1 pulse = 1 revolution), then smooth.
  float rpm = frequency * 60.0f;
  float rpmAvg = movingAverageRPM(rpm);

  // Force obvious zero / near-zero conditions to read zero wind.
  if (count == 0 || rpmAvg <= ANEMO_ZERO_RPM_THRESHOLD) {
    windSpeed = 0.0f;
  } else {
    windSpeed = (rpmAvg - B_CAL) / A_CAL;
    if (windSpeed < 0.0f) {
      windSpeed = 0.0f;
    }
  }

  lastReadTime = now;
}

String SensorAnemometer::printState(void) {
  noInterrupts();
  unsigned long live = g_pulseCount;
  interrupts();
  return "Anemo: " + String(frequency, 2) + " Hz, " +
         String(windSpeed, 2) + " m/s" +
         " (p_live=" + String(live) +
         ", p_win=" + String(lastPulseCount) +
         ", dt=" + String(lastElapsedMs) + " ms)";
}

size_t SensorAnemometer::writeDataBytes(unsigned char * buffer, size_t idx) {
  float * data_slot = (float *) &buffer[idx];
  data_slot[0] = frequency;
  data_slot[1] = windSpeed;
  idx += 2 * sizeof(float);

  noInterrupts();
  unsigned long live = g_pulseCount;
  interrupts();
  int * int_slot = (int *) &buffer[idx];
  int_slot[0] = (int)live;
  return idx + sizeof(int);
}
