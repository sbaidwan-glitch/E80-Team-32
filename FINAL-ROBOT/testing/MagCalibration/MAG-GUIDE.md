# Magnetometer Calibration Guide - E80 Team 32

## Why Calibrate?

The magnetometer measures Earth's magnetic field to compute a compass heading.
But motors, batteries, screws, and other metal on the robot create their own
magnetic fields that distort the readings. Without calibration, your heading
can be off by 10-20+ degrees in certain directions, causing the robot to veer
off course.

Calibration finds two corrections:
- **Hard iron offsets** — constant magnetic fields from permanent magnets
  nearby (motors, battery). These shift the measurement circle off-center.
- **Soft iron matrix** — distortion from ferromagnetic materials (steel frame,
  screws). These squish the circle into an ellipse.

After calibration, plotting mx vs my during a 360-degree spin should produce
a clean circle centered at the origin.

## Overview of the Process

```
Step 1: Log raw (uncalibrated) mag data while spinning outside
Step 2: Run Mag_Calibration.ino + Mag_Calibration.m to get coefficients
Step 3: Paste coefficients into SensorIMU.h
Step 4: Log calibrated data while spinning again to verify
Step 5: Generate comparison plots
```

## Files You Need

These already exist in the repo:

| File | Location | Purpose |
|------|----------|---------|
| `Mag_Calibration.ino` | `E80 Github/Mag_Calibration/` | Teensy firmware for MATLAB-controlled calibration |
| `Mag_Calibration.m` | `E80 Github/MATLAB/` | MATLAB script that computes calibration values |
| `MagLogger.ino` | `FINAL-ROBOT/testing/MagCalibration/` | Standalone mag data logger (this folder) |
| `SensorIMU.h` | `FINAL-ROBOT/libraries/main/` | Where you paste calibration values (lines 63, 66-68) |

## Step 1: Verify IMU Works (Quick Check)

Before going outside, verify the magnetometer is alive:

1. Upload `MagLogger.ino` from this folder
2. Open Serial Monitor at **115200 baud**
3. You should see CSV lines with changing magX/Y/Z values
4. Tilt and rotate the board — values should change smoothly
5. If all values are 0 or the IMU fails to init, check I2C wiring (SDA=18, SCL=19)

## Step 2: Collect Uncalibrated Data

1. Insert an SD card (formatted FAT32)
2. Upload `MagLogger.ino`
3. **Go outside, far from buildings** (steel/rebar distorts the field)
4. Power with battery
5. Hold the robot **upright** (as it would sit on the water)
6. **Slowly rotate 360 degrees** while standing in place (~10 seconds)
7. The data is saved as `MAG_XXXX.CSV` on the SD card
8. Pull the SD card — this is your **uncalibrated** data set

## Step 3: Run the E80 Calibration Tool

This uses the official E80 `Mag_Calibration.ino` + `Mag_Calibration.m` pair.

### 3a. Upload the calibration firmware

1. Open `E80 Github/Mag_Calibration/Mag_Calibration.ino` in Arduino IDE
2. Upload to Teensy 4.0
3. Note the Teensy's serial port name (under Tools > Port in Arduino IDE)
4. **Close the Serial Monitor** — MATLAB needs exclusive serial access

### 3b. Edit the MATLAB script

Open `E80 Github/MATLAB/Mag_Calibration.m` and update the serial port on
**line 36**:

```matlab
% Change this to match YOUR Teensy port:
s = serial('/dev/tty.usbmodemXXXXX','BaudRate',115200);
```

Find your port name from Arduino IDE (Tools > Port). It will look like:
- Mac: `/dev/tty.usbmodemXXXXXXX`
- Windows: `COM3` or similar

**MATLAB version note:** If you're on MATLAB R2020a or newer, the `serial()`
function is deprecated. Replace the serial setup with:

```matlab
s = serialport('/dev/tty.usbmodemXXXXX', 115200);
configureTerminator(s, "LF");
```

And replace `fopen(s)` / `fclose(s)` calls — `serialport` opens automatically.
You may also need to adjust the `fread` call to `read(s, 3*numSamples, 'single')`.

### 3c. Run the calibration

1. Run `Mag_Calibration.m` in MATLAB
2. When prompted, **rotate the motherboard into as many orientations as
   possible** for ~50 seconds. This means:
   - Spin horizontally (yaw)
   - Tilt forward/backward (pitch)
   - Tilt left/right (roll)
   - Try to cover all orientations — you're mapping a 3D sphere
3. Two figures will appear:
   - **Figure 1**: Raw data — should look like a sphere/ellipsoid, probably
     off-center
   - **Figure 2**: Calibrated data — should look like a sphere centered at
     the origin
4. The MATLAB Command Window will print:

```
Offset Vector:
Make sure to multiply the Y component by -1 because the IMU doesnt use the RHR
    X.XXXX
    Y.YYYY    <-- negate this value before pasting
    Z.ZZZZ

Soft Iron Compensation Matrix:
    a.aa    b.bb    c.cc
    d.dd    e.ee    f.ff
    g.gg    h.hh    i.ii
```

### 3d. IMPORTANT: Y-axis sign

The MATLAB script's output note says to **negate the Y offset**. This is
because the calibration firmware and MATLAB double-handle the Y-axis negation.
When you paste the offset into `SensorIMU.h`, **multiply the Y offset by -1**.

The soft iron matrix does NOT need sign changes.

## Step 4: Update SensorIMU.h

Open `FINAL-ROBOT/libraries/main/SensorIMU.h` and replace the placeholder
values:

**Line 63 — hard iron offsets** (remember to negate Y):
```cpp
float mag_offsets[3]        = { X.XXXX, -Y.YYYY, Z.ZZZZ };
//                                       ^^^^ negated!
```

**Lines 66-68 — soft iron compensation matrix:**
```cpp
float mag_ironcomp[3][3] =  { {  a.aa,     b.bb,     c.cc   },
                              {  d.dd,     e.ee,     f.ff   },
                              {  g.gg,     h.hh,     i.ii   } };
```

## Step 5: Verify Calibration

1. Upload `MagLogger.ino` again (it has its own copy of the calibration
   values — update those too, or just use the main E80_Project.ino)
2. Go outside and spin 360 degrees again
3. Pull the SD card — this is your **calibrated** data set

## Step 6: Generate Comparison Plots

Load both CSV files in MATLAB and generate:

### Plot 1: mx vs my scatter (calibrated vs uncalibrated)

```matlab
% Load data
uncal = readmatrix('MAG_0000.CSV');  % your uncalibrated file
cal   = readmatrix('MAG_0001.CSV');  % your calibrated file

figure;
scatter(uncal(:,2), uncal(:,3), 10, 'r', 'filled'); hold on;
scatter(cal(:,5), cal(:,6), 10, 'b', 'filled');
xlabel('magX [uTesla]');
ylabel('magY [uTesla]');
legend('Uncalibrated', 'Calibrated');
title('Magnetometer Calibration: mx vs my');
axis equal; grid on;
```

The red dots (uncalibrated) should be an off-center ellipse.
The blue dots (calibrated) should be a circle centered near the origin.

### Plot 2: heading vs time (calibrated vs uncalibrated)

```matlab
figure;
plot(uncal(:,1)/1000, uncal(:,8), 'r-'); hold on;
plot(cal(:,1)/1000, cal(:,8), 'b-');
xlabel('Time [s]');
ylabel('Heading [degrees]');
legend('Uncalibrated', 'Calibrated');
title('Heading vs Time During 360-degree Spin');
grid on;
```

The calibrated heading should ramp smoothly through 360 degrees.
The uncalibrated heading will have bumps and flat spots.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| magX/Y/Z all read 0 | IMU not initialized — check I2C wiring |
| Values don't change when rotating | Wrong I2C address or sensor not enabled |
| MATLAB can't open serial port | Close Arduino Serial Monitor first |
| MATLAB `serial()` not found | Use `serialport()` on MATLAB R2020a+ |
| Calibration figure 2 is still distorted | Rotate into more orientations (need full 3D coverage) |
| Heading jumps at certain angles after cal | Soft iron matrix may need refinement — recalibrate |
| `Mag_Calibration.m` says "too poor data" | Need more rotation coverage — tilt + spin in all directions |

## What the Calibration Values Mean

In `SensorIMU.h`:

```cpp
// Hard iron offsets — subtracted from raw readings to re-center the data
float mag_offsets[3] = { offset_x, offset_y, offset_z };

// Soft iron compensation — 3x3 matrix that reshapes ellipse -> circle
float mag_ironcomp[3][3] = { { ... }, { ... }, { ... } };
```

The pipeline in `SensorIMU.cpp` (lines 53-65):
1. Convert raw mGauss readings to uTesla (multiply by 0.1)
2. Subtract `mag_offsets` (hard iron correction)
3. Multiply by `mag_ironcomp` matrix (soft iron correction)
4. Feed corrected values into `getOrientation()` for heading

## Calibration Tips

- **Calibrate with the robot fully assembled** — motors, battery, frame, all
  sensors mounted. The calibration captures distortions from YOUR robot's
  specific hardware layout.
- **Calibrate at your test site if possible** — local magnetic environment
  (underground pipes, nearby buildings) can affect readings.
- **Recalibrate if you change hardware** — adding/moving a motor, battery, or
  metal bracket changes the magnetic environment.
- **Don't calibrate indoors** — buildings have rebar and wiring that create
  strong local magnetic fields.
- **Move slowly** — jerky movements can introduce accelerometer noise that
  affects tilt compensation.
