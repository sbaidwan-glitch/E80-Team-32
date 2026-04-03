// BPW34 photodiode + TIA test sketch for Teensy 4.0
// Reads analog voltage from the transimpedance amplifier output
// and prints to Serial Monitor every second.
// Wiring: TIA output -> pin 15 (A01 on MTB)

#define BPW34_PIN A1  // Teensy pin 15 = A01 on MTB

#define ADC_BITS 12
#define ADC_MAX  ((1 << ADC_BITS) - 1)  // 4095 for 12-bit
#define VREF     3.3f

#define PRINT_INTERVAL_MS 1000

static unsigned long lastPrintTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  analogReadResolution(ADC_BITS);
  pinMode(BPW34_PIN, INPUT);

  Serial.println("=== BPW34 Photodiode Test ===");
  Serial.print("Pin: A1 (Teensy pin 15), ");
  Serial.print(ADC_BITS);
  Serial.println("-bit ADC");
  Serial.println("Time,ADC_Raw,Voltage");
  Serial.println();

  lastPrintTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    int raw = analogRead(BPW34_PIN);
    float voltage = (float)raw * VREF / ADC_MAX;

    Serial.print(now);
    Serial.print(",");
    Serial.print(raw);
    Serial.print(",");
    Serial.println(voltage, 4);

    lastPrintTime = now;
  }
}
