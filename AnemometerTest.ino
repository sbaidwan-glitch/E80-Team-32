
#define ANEMOMETER_PIN 16
#define PRINT_INTERVAL_MS 1000

static volatile unsigned long pulseCount = 0;

static void anemometerISR(void) {
  pulseCount++;
}

static unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // wait up to 3 s for Serial Monitor to connect
  }

  pinMode(ANEMOMETER_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(ANEMOMETER_PIN), anemometerISR, RISING);

  lastPrintTime = millis();
  pulseCount = 0;

  Serial.println("=== Anemometer Test ===");
  Serial.print("Pin: ");
  Serial.println(ANEMOMETER_PIN);
  Serial.println("Anemometer test ready. Spin the cups to see pulses.\n");
}

void loop() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastPrintTime;

  if (elapsed >= PRINT_INTERVAL_MS) {
    noInterrupts();
    unsigned long count = pulseCount;
    pulseCount = 0;
    interrupts();

    float freq = (float)count / ((float)elapsed / 1000.0);

    Serial.print("Pulses: ");
    Serial.print(count);
    Serial.print("  |  Freq: ");
    Serial.print(freq, 2);
    Serial.println(" Hz");

    lastPrintTime = now;
  }
}
