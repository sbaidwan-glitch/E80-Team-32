/*
  GPS Test Sketch for E80 Team 32 - Teensy 4.0

  Tests the Adafruit Ultimate GPS (product 746) on Serial1.
  Prints lat, lon, satellites, fix quality, altitude, and speed
  to the serial monitor. Also logs to SD card as CSV.

  Hardware:
    - GPS module plugged into the motherboard GPS header
    - Power via battery (NOT USB alone — GPS needs battery voltage)
    - GPS lock LED on pin 20

  Open Serial Monitor at 115200 baud after uploading.
  Take the board OUTSIDE with clear sky view for satellite fix.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GPS.h>

#define UartSerial Serial1

// GPS lock LED
#define GPS_LOCK_LED 20
#define N_SATS_THRESHOLD 5

// How often to print a status line (ms)
#define PRINT_INTERVAL_MS 2000

Adafruit_GPS GPS(&UartSerial);

static unsigned long lastPrintTime = 0;
static bool sdReady = false;
static char logFilename[20];
static bool hadFirstFix = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {}

  // Disable motor pins -- they float on boot and the H-bridge will run them.
  const uint8_t motorPins[] = {3, 4, 5, 6, 22, 23};
  for (uint8_t i = 0; i < sizeof(motorPins); i++) {
    pinMode(motorPins[i], OUTPUT);
    digitalWrite(motorPins[i], LOW);
  }

  pinMode(GPS_LOCK_LED, OUTPUT);
  digitalWrite(GPS_LOCK_LED, LOW);

  Serial.println("=== E80 GPS Test ===");
  Serial.println("Take board OUTSIDE with clear sky view.");
  Serial.println("Power with BATTERY (not just USB).");
  Serial.println();

  // Start GPS at 9600 baud (default for Adafruit Ultimate GPS)
  GPS.begin(9600);

  // Request RMC + GGA sentences (position, altitude, satellites)
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);

  // 1 Hz update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  Serial.println("GPS initialized. Waiting for satellite fix...");
  Serial.println("(Cold start can take 30-60 seconds outdoors)");
  Serial.println();

  // Initialize SD card
  Serial.print("Initializing SD card... ");
  if (SD.begin()) {
    Serial.println("done!");
    sdReady = true;
    for (int i = 0; i < 1000; i++) {
      snprintf(logFilename, sizeof(logFilename), "GPS_%04d.CSV", i);
      if (!SD.exists(logFilename)) break;
    }
    File file = SD.open(logFilename, FILE_WRITE);
    if (file) {
      file.println("Time_ms,Lat,Lon,Nsats,Fix,FixQuality,Altitude_m,Speed_knots,Angle_deg");
      file.close();
      Serial.print("Logging to: ");
      Serial.println(logFilename);
    } else {
      sdReady = false;
    }
  } else {
    Serial.println("failed! (continuing without logging)");
  }

  Serial.println();
  Serial.println("Time_ms,Lat,Lon,Nsats,Fix,FixQuality,Altitude_m,Speed_knots,Angle_deg");

  lastPrintTime = millis();
}

void loop() {
  // Must call GPS.read() every loop iteration to consume serial bytes.
  // The GPS sends data continuously — if you don't read fast enough,
  // the serial buffer overflows and you lose sentences.
  GPS.read();

  // Check if a complete NMEA sentence has arrived
  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA())) {
      return;  // bad sentence, wait for next
    }
  }

  // Update GPS lock LED
  if (GPS.satellites >= N_SATS_THRESHOLD) {
    digitalWrite(GPS_LOCK_LED, HIGH);
  } else {
    digitalWrite(GPS_LOCK_LED, LOW);
  }

  // Print/log at regular intervals
  unsigned long now = millis();
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;

    // First fix notification
    if (GPS.fix && !hadFirstFix) {
      hadFirstFix = true;
      Serial.println();
      Serial.println("*** GPS FIX ACQUIRED ***");
      Serial.println();
    }

    // Status header
    if (!GPS.fix) {
      Serial.print("[NO FIX] sats=");
      Serial.print(GPS.satellites);
      Serial.println(" — need clear sky, wait...");
      return;
    }

    // Print CSV line
    Serial.print(now);
    Serial.print(",");
    Serial.print(GPS.latitudeDegrees, 7);
    Serial.print(",");
    Serial.print(GPS.longitudeDegrees, 7);
    Serial.print(",");
    Serial.print(GPS.satellites);
    Serial.print(",");
    Serial.print(GPS.fix);
    Serial.print(",");
    Serial.print(GPS.fixquality);
    Serial.print(",");
    Serial.print(GPS.altitude, 1);
    Serial.print(",");
    Serial.print(GPS.speed, 2);
    Serial.print(",");
    Serial.println(GPS.angle, 1);

    // Log to SD
    if (sdReady) {
      File file = SD.open(logFilename, FILE_WRITE);
      if (file) {
        file.print(now);
        file.print(",");
        file.print(GPS.latitudeDegrees, 7);
        file.print(",");
        file.print(GPS.longitudeDegrees, 7);
        file.print(",");
        file.print(GPS.satellites);
        file.print(",");
        file.print(GPS.fix);
        file.print(",");
        file.print(GPS.fixquality);
        file.print(",");
        file.print(GPS.altitude, 1);
        file.print(",");
        file.print(GPS.speed, 2);
        file.print(",");
        file.println(GPS.angle, 1);
        file.close();
      }
    }
  }
}
