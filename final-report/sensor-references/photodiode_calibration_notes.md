# Photodiode Calibration Notes

## Project Context

This calibration supports Hypothesis 2 of the E80 research project: measuring solar irradiance as a function of distance from shore at Dana Point using an autonomous surface vehicle. The goal is to help offshore energy planners determine optimal solar panel placement.

**Why irradiance, not lux:** Solar panels convert total radiant power (W/m²) across their spectral range, not just visible light weighted by human eye response (lux). The weather station reference data reports solar irradiance in W/m², so the calibration must map sensor output to irradiance.

## Sensors

### VEML6030 (Digital Ambient Light Sensor)

- **Datasheet:** `veml6030.pdf`
- **Breakout board:** SparkFun SEN-15436 (`vemlwithmount.pdf`)
- **Interface:** I2C (address 0x10, ADDR pin tied to GND)
- **Output:** Digital lux value (16-bit)
- **Range:** 0 to 120,000 lux (configurable via gain/integration time)
- **Spectral response:** Peaks ~550-560 nm, approximates human eye V(λ) curve
- **Connection to Teensy 4.0:**
  - SDA (pin 2) → Teensy pin 18 (SDA0)
  - SCL (pin 5) → Teensy pin 19 (SCL0)
  - 4.7 kΩ pull-ups on both lines to 3.3V
  - 100 nF bypass cap on VDD
  - Powered from Teensy 3.3V rail

#### Gain and Integration Time (Configured via I2C)

Gain and integration time are set by writing to the configuration register (command code 0x00) over I2C from the Teensy. The SparkFun library provides simple functions for this.

**Configuration Register (0x00) Relevant Bits:**
- ALS_GAIN (bits 12:11): `00` = x1, `01` = x2, `10` = x1/8, `11` = x1/4
- ALS_IT (bits 9:6): `1100` = 25 ms, `1000` = 50 ms, `0000` = 100 ms, `0001` = 200 ms, `0010` = 400 ms, `0011` = 800 ms

**Recommended settings for outdoor sunlight (~100,000 lux):**
- Gain: x1/8 (to avoid ADC saturation)
- Integration time: 50 ms (or 25 ms if still clipping)
- If raw reading maxes at 65535, reduce integration time further

**Example Teensy code:**

```cpp
#include <SparkFun_Ambient_Light_Sensor.h>

SparkFun_Ambient_Light light(0x10);  // ADDR tied to GND

void setup() {
  Wire.begin();
  light.begin();
  light.setGain(0.125);    // ALS_GAIN x1/8 for bright sunlight
  light.setIntegTime(50);  // 50 ms integration time
}

void loop() {
  long lux = light.readLight();
  // log lux value with timestamp
}
```

**Resolution at different settings (from datasheet):**

| ALS_GAIN | ALS_IT (ms) | Resolution (lx/bit) | Max Lux |
|----------|------------|---------------------|---------|
| x2       | 800        | 0.0042              | 275     |
| x2       | 100        | 0.0336              | 2,201   |
| x1       | 100        | 0.0672              | 4,402   |
| x1/4     | 100        | 0.2688              | 17,607  |
| x1/8     | 100        | 0.5376              | 35,214  |
| x1/8     | 50         | 1.0752              | 70,429  |
| x1/8     | 25         | 2.1504              | 140,858 |

**Calibration purpose:** Establish mapping from VEML6030 lux → solar irradiance (W/m²).

---

### BPW34 (Silicon PIN Photodiode)

- **Datasheet:** `bpw34.pdf`
- **Type:** Analog photodiode (current output)
- **Spectral range:** 430 to 1100 nm (peak sensitivity at 900 nm, near-IR)
- **Radiant sensitive area:** 7.5 mm²
- **Half-sensitivity angle:** ±65°
- **Reverse light current:** 50 µA typ at 1 mW/cm², λ = 950 nm, VR = 5V
- **Interface:** Transimpedance amplifier (TIA) circuit → analog voltage to Teensy

#### TIA Circuit Design (from proposal)

- **Op-amp:** Single-rail 6004, powered by +5V
- **Feedback resistor (Rf):** 20 kΩ
- **Design rationale:** At assumed irradiance of 2 mW/cm², photocurrent ~100 µA. Vout = Id × Rf = 100 µA × 20 kΩ = 2V (within Teensy 0–3.3V ADC range)
- **Bypass capacitors:** 10 µF and 100 nF
- **Output:** Analog voltage read by Teensy pin A8

**Calibration purpose:** Establish mapping from BPW34 TIA output voltage → solar irradiance (W/m²).

**Note on spectral response:** The BPW34 peaks at 900 nm (near-IR), which is quite different from the VEML6030's visible-light-weighted response. This means the BPW34 is more sensitive to the near-IR portion of sunlight, and its response may actually correlate more closely with total broadband solar irradiance than the VEML6030's lux output does.

---

## Calibration Procedure

### Reference Data Source

**NWS Claremont Weather Station (CMOC1):**
https://www.weather.gov/wrh/timeseries?site=CMOC1

- Reports solar irradiance (W/m²)
- Default: last 72 hours of data
- Historical: up to 30 days
- Downloadable as CSV or XLS
- Use "Advanced Options" to select "Solar Radiation" and set date range

### Setup

1. Mount both sensors **flat, facing straight up** outside the dorm with a clear, unobstructed view of the sky
2. No overhanging eaves, trees, or buildings casting shadows
3. Sensors should be level (not tilted against a wall or window)
4. Connect both sensors to the Teensy as per the electrical schematics in the proposal
5. Power Teensy via USB (laptop or power bank)

### Data Collection

- Log both sensors + timestamp every 30–60 seconds
- Run for a full day (sunrise to sunset) on a **clear day**
- Download the CMOC1 weather station CSV for the same time period
- Weather station may report at coarser intervals (5–15 min) — average sensor data over matching intervals for comparison

### Data Analysis

1. Time-align sensor data with weather station data
2. Plot VEML6030 lux vs. weather station irradiance (W/m²)
3. Plot BPW34 voltage vs. weather station irradiance (W/m²)
4. Fit calibration curves (likely linear for the BPW34 TIA, possibly linear for VEML6030 over the solar range)
5. Report R² values and residuals

### Expected Calibration Curves

- **VEML6030:** Lux (sensor) vs. W/m² (station) — should be roughly linear since both respond to sunlight, but the spectral weighting difference (V(λ) vs. broadband) means the slope may shift with solar angle/atmospheric conditions
- **BPW34:** Voltage (sensor) vs. W/m² (station) — the TIA is inherently linear (Vout = Id × Rf), and the BPW34's broader spectral response may track total irradiance more faithfully

---

## Known Error Sources

| Source | Magnitude | Notes |
|--------|-----------|-------|
| Temperature drift (BPW34) | ~3-5% over a day | Photocurrent increases ~0.1%/K. Claremont day: ~15°C morning to ~30°C afternoon. Noted as error source but not corrected. |
| Temperature drift (VEML6030) | Small | Has built-in temperature compensation per datasheet |
| Angular sensitivity | Moderate | Neither sensor has a true cosine corrector like a pyranometer. Both drop off at high angles from normal. On the robot, wave-induced tilting adds noise (mitigated by 5-min averaging). |
| Weather station distance | Variable | CMOC1 is in Claremont, not at the dorm. Cloud cover differences between the two locations could cause discrepancies. Choose a clear day to minimize this. |
| Spectral mismatch | Systematic | VEML6030 is V(λ)-weighted (visible), BPW34 peaks at 900 nm (near-IR). Neither perfectly matches a pyranometer's flat spectral response. This is a constant offset, not random error. |
| Weatherproofing | — | Sensors are not waterproof. Avoid rainy/dewy conditions. Consider a clear cover (plastic wrap, acrylic) but note it will attenuate some light consistently. |
| ADC saturation | Avoidable | Use low gain (x1/8) and short integration (25-50 ms) on VEML6030. Check that BPW34 TIA output stays below 3.3V in full sun. |

---

## Calibration Step-by-Step (Detailed)

### Why Calibration Is Needed

Neither sensor directly outputs solar irradiance in W/m^2, which is the quantity relevant to solar panel placement (Hypothesis 2).

The VEML6030 outputs lux, which is radiant power weighted by the human eye's photopic response V(lambda). Lux emphasizes green light (~555 nm) and ignores infrared and ultraviolet -- wavelengths that solar panels still convert to electricity. There is no fixed lux-to-W/m^2 ratio because the conversion depends on the light source's spectrum, which changes throughout the day as the sun's angle and atmospheric path length vary.

The BPW34 produces a photocurrent converted to voltage by the transimpedance amplifier (Vout = I_diode * R_f). While this voltage is proportional to incident optical power, the proportionality constant depends on the BPW34's spectral sensitivity (peaked at 900 nm), the solar spectrum, and the specific R_f chosen. Without calibration, we have volts but not W/m^2.

A calibration curve maps each sensor's output to a known irradiance reference, allowing us to convert field measurements from the robot into physically meaningful units.

### Reference Standard

The NWS Claremont weather station (CMOC1) provides solar irradiance measured by a calibrated pyranometer:

https://www.weather.gov/wrh/timeseries?site=CMOC1

This station reports global horizontal irradiance (GHI) in W/m^2 -- the total solar power incident on a horizontal surface, which is exactly what a flat-mounted solar panel would receive. Data is downloadable as CSV.

### Step 1: Configure the VEML6030 for Outdoor Use

Before deploying, set the gain and integration time via I2C so the sensor does not saturate in direct sunlight. Full sunlight is around 100,000 lux. The default gain (x1) will saturate the 16-bit ADC. Set gain to x1/8 and integration time to 25 ms, which allows reading up to around 140,000 lux with a resolution of 2.15 lux/bit -- more than adequate for outdoor readings. This is done in the Teensy firmware by calling light.setGain(0.125) and light.setIntegTime(25) using the SparkFun library. These are I2C writes to the VEML6030 configuration register (0x00).

### Step 2: Verify the BPW34 TIA Will Not Clip

The TIA output is Vout = I_diode * R_f. With R_f = 20 kOhm, at 2 mW/cm^2 (typical peak sunlight irradiance at 950 nm), I_diode is approximately 100 uA, giving Vout of approximately 2.0 V, which is within range. At 5 mW/cm^2 (very bright, high solar noon), I_diode is approximately 250 uA, giving Vout of approximately 5.0 V, which clips at the op-amp rail or the Teensy 3.3V ADC limit. If clipping is observed during the test (ADC reads its max value), reduce R_f to 10 kOhm. For this initial calibration, 20 kOhm should be fine for most of the day, but check peak noon readings.

### Step 3: Write the Teensy Logging Firmware

The firmware should initialize both sensors (VEML6030 over I2C, BPW34 via analogRead on pin A8), read both sensors simultaneously at a fixed interval of every 30 seconds, record a timestamp with each reading using millis() synchronized to a known clock time at startup, and log data to SD card or serial output to a laptop. Each log entry should be a CSV line with the format: timestamp_ms, veml6030_lux, bpw34_adc_raw, bpw34_voltage. Convert the BPW34 ADC reading to voltage using: voltage = adc_raw * (3.3 / 4095.0) for the Teensy 4.0 12-bit ADC.

### Step 4: Mount the Sensors

Place both sensors flat, facing straight up on a level surface outside the dorm such as a windowsill ledge, a flat railing, or a small board. Ensure no obstructions above -- no eaves, tree branches, or building overhangs that would cast shadows at any point during the day. Both sensors should be at the same location, side by side, so they experience identical lighting conditions. Secure them so they do not shift or get knocked over throughout the day.

The reason for facing straight up is that the weather station pyranometer measures global horizontal irradiance -- total solar power hitting a horizontal surface. Matching this orientation ensures the calibration is valid. If the sensors were tilted, they would weight direct sunlight differently depending on sun angle, and the calibration would only be valid for that specific tilt.

### Step 5: Collect Data

Choose a clear, cloudless day. Clouds cause rapid irradiance fluctuations that make it hard to align sensor data with weather station data, which may be averaged over longer intervals. Start logging at sunrise and stop at sunset, or as long as practical -- at minimum, capture the noon peak plus 2 to 3 hours on either side. Note the exact clock time when logging starts so sensor timestamps can be converted to real time. Do not move or touch the sensors during the run.

A full sunrise-to-sunset run gives the most useful calibration because it spans the full irradiance range (0 to around 1000 W/m^2) and it captures how the lux-to-irradiance ratio changes with solar angle, since the spectrum shifts redder at sunrise and sunset, affecting the VEML6030 differently than the BPW34.

### Step 6: Download the Reference Data

Go to the CMOC1 time series page. Use Advanced Options, select Solar Radiation, and set the date range to match the collection day. Download as CSV. Note the time zone and reporting interval (likely every 5 to 15 minutes).

### Step 7: Time-Align the Data

The sensor logs every 30 seconds, but the weather station reports every 5 to 15 minutes. To compare them, convert all timestamps to the same time zone and format. For each weather station data point, average all sensor readings that fall within that reporting interval. This produces matched pairs of (average sensor reading, weather station irradiance). For example, if the weather station reports 850 W/m^2 at 12:00 PM averaged over 12:00 to 12:15, average all sensor readings from 12:00 to 12:15 to get one VEML6030 lux value and one BPW34 voltage value.

### Step 8: Plot and Fit Calibration Curves

For the VEML6030, plot weather station irradiance (W/m^2) on the x-axis and VEML6030 reading (lux) on the y-axis. Fit a linear regression: lux = m * irradiance + b. Report the slope (m), intercept (b), and R^2 value.

For the BPW34, plot weather station irradiance (W/m^2) on the x-axis and BPW34 TIA output voltage (V) on the y-axis. Fit a linear regression: voltage = m * irradiance + b. Report the slope (m), intercept (b), and R^2 value.

Both should be approximately linear. The BPW34 TIA is inherently linear because Vout = I * R_f, and photocurrent is linear with irradiance per Fig. 3 in the BPW34 datasheet. The VEML6030 should also be roughly linear since lux and irradiance both scale with incident power, though the spectral weighting difference may introduce slight nonlinearity at extreme solar angles.

### Step 9: Derive the Conversion Equations

Invert the calibration fits to get the equations used on the robot:

- VEML6030: irradiance (W/m^2) = (lux - b) / m
- BPW34: irradiance (W/m^2) = (voltage - b) / m

These equations go into the Teensy firmware for the field deployment at Dana Point.

### Step 10: Validate (If Time Permits)

Repeat the measurement on a different day and apply the calibration equations. Compare the sensor-predicted irradiance to the weather station's actual irradiance for that second day. The residuals indicate the calibration's accuracy and robustness.

---

## Expected Outcome

The two sensors provide complementary measurements. The VEML6030 has a spectral weighting based on V(lambda), peaking at 555 nm in the visible range. It should correlate well with irradiance but the spectral mismatch may cause scatter at sunrise and sunset. Its advantage is a digital output with no analog noise and built-in temperature compensation. The BPW34 has a broadband spectral response peaking at 900 nm in the near-IR. It will likely show a tighter correlation with total solar irradiance since near-IR tracks total solar power more linearly. Its advantage is broader spectral coverage that is closer to what a solar panel actually sees.

Having both calibration curves allows cross-validation: if one sensor gives an anomalous reading on the robot, the other provides a check. The two different spectral responses also let you characterize whether the solar spectrum is shifting with distance from shore, for example due to water vapor or sea spray attenuation.

---

## Error Budget Summary

| Source | Est. Effect | Mitigation |
|--------|------------|------------|
| Weather station distance from dorm | Could differ if patchy clouds | Choose a clear day |
| Temperature drift (BPW34) | ~3-5% over the day | Note as systematic error; BPW34 has +0.1%/K on photocurrent |
| Temperature drift (VEML6030) | Small | Built-in temperature compensation |
| Angular response mismatch | Systematic at low solar angles | Sensor is not cosine-corrected like a pyranometer; sunrise/sunset data may deviate |
| ADC saturation | Data loss at peak | Verify settings in Steps 1-2 before the full run |
| Time alignment error | Smears data | Use 30-sec logging to allow fine averaging |

---

## Concise Summary (Report-Ready)

Neither sensor directly outputs solar irradiance in W/m^2. The VEML6030 outputs lux, which weights light by the human eye response and ignores IR/UV that solar panels still use. The BPW34 outputs a voltage proportional to photocurrent, but the proportionality depends on its spectral sensitivity and the chosen feedback resistor. The calibration maps both outputs to irradiance using the Claremont weather station (CMOC1) pyranometer as the reference standard.

First, configure the VEML6030 to gain x1/8 and 25 ms integration time via the SparkFun library on the Teensy so it does not saturate in full sunlight. Verify the BPW34 TIA with Rf = 20 kOhm will not clip -- at 2 mW/cm^2, Vout is about 2V which is fine, but at very high noon irradiance it could exceed 3.3V, in which case reduce Rf to 10 kOhm. Write Teensy firmware that reads both sensors every 30 seconds and logs CSV lines of timestamp, lux, and voltage to SD card or serial.

Mount both sensors flat, facing straight up, side by side, on a level surface outside with no overhead obstructions. This matches the weather station's horizontal pyranometer orientation. Run data collection on a clear cloudless day from sunrise to sunset, noting the exact start time for timestamp synchronization. Download the CMOC1 solar radiation CSV for the same day.

To analyze, time-align the data by averaging all sensor readings within each weather station reporting interval (typically 5-15 min) to produce matched pairs. Plot lux vs. irradiance for the VEML6030 and voltage vs. irradiance for the BPW34, fit linear regressions to both, and report slope, intercept, and R^2. Invert these fits to get conversion equations: irradiance = (sensor_output - b) / m. These equations go into the Teensy firmware for the Dana Point deployment. If time permits, validate on a second day by applying the calibration and comparing predicted vs. actual irradiance.

Key error sources include temperature drift on the BPW34 (~3-5% over a day, not corrected), angular response mismatch since neither sensor has a cosine corrector like a pyranometer, spectral mismatch between the sensors and broadband irradiance, and potential weather differences between the dorm and the CMOC1 station location. Choosing a clear day and using 30-second logging for fine time averaging mitigates the largest sources of uncertainty.
