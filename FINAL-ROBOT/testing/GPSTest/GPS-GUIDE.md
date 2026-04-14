# GPS Test Guide - E80 Team 32

## Hardware Setup

The GPS module is the **Adafruit Ultimate GPS** (product 746) — the small blue board
with a ceramic patch antenna on top.

### Connecting to the Motherboard

1. The GPS plugs into the 9-pin header on the motherboard labeled "GPS"
2. Make sure the orientation matches the photo in the lab handout (Section 4.2,
   Figure 1) — the antenna (square metallic patch) faces UP
3. Use standoffs/screws to secure it if available
4. The GPS connects via **Serial1 (UART)** — this is separate from the I2C bus
   (SDA/SCL) used by the IMU and VEML6030, so there are no conflicts

### Power

**IMPORTANT: Power the motherboard with the BATTERY, not just USB.**
The GPS module is powered through the motherboard's voltage regulator.
USB alone may not provide enough current for a reliable GPS fix.

You can have USB plugged in at the same time (for serial monitor) — just make
sure the battery is also connected.

## Step-by-Step: Running the GPS Test

### 1. Upload the Sketch

1. Open `GPSTest.ino` in Arduino IDE
2. Select Board: **Teensy 4.0**
3. Make sure the `libraries/` folder is on your Arduino library path
   (it needs `Adafruit_GPS`)
4. Compile and upload

### 2. Go Outside

The GPS **will not work indoors**. It needs a direct line of sight to satellites.

- Go outside, away from tall buildings if possible
- Overcast sky is fine — clouds don't block GPS signals
- Standing next to a building wall is okay, open field is better

### 3. Open Serial Monitor

1. Open Serial Monitor in Arduino IDE (or any serial terminal)
2. Set baud rate to **115200**
3. You will see:
   ```
   === E80 GPS Test ===
   GPS initialized. Waiting for satellite fix...
   ```

### 4. Wait for Fix

- **Cold start** (first time, or long time since last use): 30-60 seconds
- **Warm start** (used recently, has coin cell battery): 5-15 seconds
- Watch the output — it will print `[NO FIX] sats=X` until enough satellites
  are acquired
- The GPS lock LED (pin 20, D4 on the motherboard) will:
  - Stay OFF with 0 satellites
  - Blink proportionally as satellites are found
  - Go SOLID when nsats >= 5

### 5. Read the Data

Once you have a fix, the serial monitor prints CSV lines:

```
Time_ms,Lat,Lon,Nsats,Fix,FixQuality,Altitude_m,Speed_knots,Angle_deg
45230,33.4621234,-117.7113456,7,1,1,25.3,0.12,180.5
```

| Column       | Meaning                                        |
|--------------|------------------------------------------------|
| Time_ms      | Milliseconds since Teensy powered on            |
| Lat          | Latitude in decimal degrees (+ = North)         |
| Lon          | Longitude in decimal degrees (- = West)         |
| Nsats        | Number of satellites being tracked              |
| Fix          | 1 = have a position fix, 0 = no fix            |
| FixQuality   | 0 = invalid, 1 = GPS fix, 2 = DGPS fix        |
| Altitude_m   | Altitude above mean sea level in meters         |
| Speed_knots  | Speed over ground in knots                      |
| Angle_deg    | Course heading in degrees (from GPS movement)   |

### 6. SD Card Logging

If an SD card is inserted, data is also saved as a CSV file (`GPS_0000.CSV`,
`GPS_0001.CSV`, etc.). You can pull the SD card and open the file in MATLAB or
Excel.

## What Good Data Looks Like

- **Nsats >= 5**: Accurate to within a few meters
- **Nsats >= 7**: Very good accuracy
- **Lat/Lon for Dana Point**: Lat ~ 33.46, Lon ~ -117.71
- **Lat/Lon for HMC campus**: Lat ~ 34.11, Lon ~ -117.71
- **Fix = 1**: You have a valid position
- If standing still, Speed should be near 0 and Lat/Lon should barely change

## Troubleshooting

| Problem                          | Fix                                          |
|----------------------------------|----------------------------------------------|
| No output at all                 | Check baud rate is 115200, check USB cable    |
| Stuck at `[NO FIX] sats=0`      | Go outside! GPS does not work indoors         |
| `sats` increases but no fix      | Wait longer, move to open area                |
| `sats` stays at 0 outdoors       | Check GPS is plugged in correctly, check battery power |
| Lat/Lon are 0.0000000            | No fix yet — wait for more satellites         |
| SD card logging failed           | Check SD card is inserted, formatted FAT32    |
| LED never turns on               | Check GPS wiring and battery connection       |

## After Testing: Integration with Main Project

The GPS is already integrated into `E80_Project.ino`:
- `gps.read(&GPS)` runs every loop iteration (line 262)
- Lat, lon, and nsats are logged to the binary log every ~99ms
- Station arrival messages now print GPS coordinates to serial
- The `XYStateEstimator` converts lat/lon to x/y meters (once you fill in
  the calculation in `XYStateEstimator.cpp:33`)

### Setting the Origin for XY Conversion

In `XYStateEstimator.h` (lines 44-45), the origin is set to HMC campus:
```cpp
const float origin_lat = 34.106465;
const float origin_lon = -117.712488;
```

For your Dana Point deployment, update these to your launch point coordinates.
Use the GPS test to get the exact lat/lon of your launch site, then plug those
values in before your final deployment.

### XY Conversion Formulas (Section 4.3 of Lab Handout)

The formulas to add in `XYStateEstimator.cpp` (the empty section at line 33):
```cpp
// Convert heading to yaw (CCW from East, in radians)
state.yaw = -imu_state_p->heading * PI / 180.0;

// Latitude change -> Y (north-south distance)
float latChange = (gps_state_p->lat - origin_lat) * PI / 180.0;
state.y = RADIUS_OF_EARTH_M * latChange;

// Longitude change -> X (east-west distance, corrected for latitude)
float lonChange = (gps_state_p->lon - origin_lon) * PI / 180.0;
state.x = RADIUS_OF_EARTH_M * lonChange * cos(origin_lat * PI / 180.0);
```
