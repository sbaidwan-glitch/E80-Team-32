# Final Report Worksheet, Team 32 Draft

## Outline

The seven sections planned for the final report:

1. Abstract
2. Introduction
3. Sensor Design (per sensor)
4. Robot Design
5. Deployment Details
6. Results
7. Conclusion
8. Acknowledgements
9. References

## Template

### Abstract

**Main gist of the project.** We built an autonomous surface vehicle to measure wind speed, wind direction, and solar irradiance versus distance from shore at Dana Point. The deployment took place inside Dana Cove, a sheltered embayment within Dana Point Harbor, so the available transect (about 145 m) fell inside the breakwater. We could not test the original gradient hypotheses at this scale, so the project pivoted to a quantitative comparison of wind and solar energy density at the deployment site, at a contemporaneous open-coast reference, and at a nearby inland reference.

**Significant quantitative result(s).**

* In-cove mean wind power density: 6.2 W/m^2 (mean wind 2.16 m/s).
* Open-coast wind reference (WeatherSpark Dana Point, 09 to 11 PDT): 72.8 W/m^2 (mean 4.92 m/s, 11 mph).
* Solar irradiance over the deployment window (CIMIS Station 241): 475 W/m^2.
* Solar / measured wind: 77x.
* Solar / open-coast wind: 6.5x.
* Open-coast wind / cove wind: 11.7x (sheltering ratio).

### Introduction

**Key concepts to introduce, with references.**

* **Nearshore-to-offshore wind gradient.** Air over rough nearshore water experiences greater drag than air over smooth open water. *Ref: NOAA NDBC, "How does wind speed over the ocean compare with wind speed over the land?"*
* **Solar reflectance and shallow-water albedo.** Sand albedo (about 0.15 to 0.40) and shallow sandy bottoms contribute to nearshore irradiance, while deep-water albedo (about 0.04 to 0.10) dominates offshore. *Refs: NASA Albedo Values; Maritorena et al., 1994, "Diffuse reflectance of oceanic shallow waters."*
* **Wind power density.** P/A = 0.5 × ρ × v^3 (W/m^2). The cube law makes wind power density very sensitive to wind speed. ρ = 1.225 kg/m^3 at sea level.
* **Coastal sheltering.** Breakwaters, headlands, and piers attenuate wind locally, so energy resource maps need to reflect site-specific topography.
* **Cup anemometry, photodiode irradiance measurement, contactless rotary position sensing, GPS positioning, IMU compass heading.** *Refs: BPW34 datasheet (Vishay), AH9246 datasheet (Diodes Inc.), MLX90316 datasheet (Melexis), LSM303AGR datasheet (STMicroelectronics).*

### Sensor Design

**Sensors on the robot:**

1. Cup anemometer (3D-printed cups plus AH9246 hall-effect sensor) for wind speed.
2. Weather vane (3D-printed vane plus MLX90316 contactless rotary position sensor) for wind direction.
3. BPW34 photodiode plus transimpedance amplifier for solar irradiance.
4. GPS receiver for position.
5. LSM303AGR IMU for compass heading (used for vane fusion).

#### Sensor 1: Cup Anemometer

**Expected input range.** 0 to 10 m/s typical for nearshore SoCal sea breeze. Deployment-day range was 0 to 4 m/s.

**Sensor selection.** AH9246 omnipolar hall sensor for its 3.3 V single-supply operation, open-drain output (drives the Teensy interrupt directly), and roughly 5 µA quiescent current. The cup rotor is 3D-printed in PLA with three cups at 120 degrees on a 5 mm shaft. One magnet on one cup arm produces one pulse per revolution.

**Circuit.** AH9246 output pulled to 3.3 V via 10 kΩ; signal goes to Teensy pin 14 as a digital input with a falling-edge interrupt. Software debounce filter rejects pulses within 10 ms of each other.

**Software modifications.** Custom `SensorAnemometer` class extending the project's `DataSource` pattern. 5-sample moving average smooths the computed RPM. Wind speed is computed as `v = (RPM + 1.71) / 21.1` (m/s).

**Validation/calibration.** Wind tunnel calibration at HMC pre-deployment. The test sketch (`testing/AnemometerTest.ino`) was used to log RPM and pulse counts to `ANEMO###.CSV` files on the SD card while the rotor was held at known wind speeds. A linear fit produced slope `A_CAL = 21.1` and intercept `B_CAL = -1.71`. The deployment-day field validation showed the anemometer logged through all three Dana logs without dropouts, with transient gusts and lulls in the 0 to 4 m/s range visible in the time series.

#### Sensor 2: Weather Vane (MLX90316)

**Expected input range.** 0 to 360 degrees (full mechanical revolution).

**Sensor selection.** MLX90316 contactless magnetic rotary position sensor, picked for wear-free operation, full 360 degree absolute range, and analog output suitable for ADC sampling.

**Circuit.** MLX90316 supplied at 5 V. Analog output (0 to 5 V) divided by 18 kΩ / 30 kΩ to bring it into the Teensy 4.0's 0 to 3.3 V ADC range. Read on pin 15 (A1) at 12-bit resolution.

**Software modifications.** Custom `SensorWeatherVane` class with 10-sample moving-average smoothing. Linear voltage-to-angle map: `angle = 360 × (V - VMIN) / (VMAX - VMIN)` with VMIN = 0.094 V and VMAX = 3.000 V. Output wrapped to [0, 360) degrees.

**Validation/calibration.** VMIN and VMAX were measured pre-deployment by manually sweeping through one full rotation. The mechanical zero (vane points along the bow direction) was measured at V = 0.52 V, which corresponds to a raw angle of 52.8 degrees. We subtract that from each sample to put 0 degrees equal to wind from the bow in the boat-relative frame. Earth-frame wind direction is then `(vane_angle - 52.8 + headingIMU) mod 360`. The deployment-day wind rose over 14,362 filtered samples (wind speed at least 1.5 m/s) shows a clear directional concentration in the southern quadrant, within about 35 degrees of WeatherSpark's reported regional direction of 215 degrees. The residual offset is consistent with magnetometer hard-iron error and local cove channeling.

* Plot to attach: wind rose, count plus speed-weighted. Source: `data-deployment/dana-raw/dana_figF_windrose.png`.

#### Sensor 3: BPW34 Photodiode plus Transimpedance Amplifier

**Expected input range.** Solar irradiance 0 to 1000 W/m^2 (full sun). Deployment-day morning range was about 400 to 550 W/m^2 per the CIMIS reference.

**Sensor selection.** BPW34 silicon PIN photodiode for its wide spectral range (430 to 1100 nm), 7.5 mm^2 active area, and low cost. MCP602 op-amp as the TIA.

**Circuit.** Photodiode in photovoltaic mode (zero-bias) feeds the op-amp inverting input. Feedback is 1 MΩ in parallel with about 100 pF for stability. Output goes to Teensy pin 16 (A02) and into the 12-bit ADC.

**Software modifications.** Read at 10 Hz by `ADCSampler`. ADC-to-voltage conversion: `V = ADC × 3.3 / 4095`.

**Validation/calibration.** Pre-deployment lab calibration linked photodiode voltage to known illumination and produced a rough linear relation. The lab cal is documented but not tightly bounded. As a field sanity check, BPW34 voltage during the deployment averaged about 0.52 V at clear sky and dropped to about 0.42 V (around a 25 percent reduction) during a brief cloud transit in LOG009 minute 12. That transient demonstrates the sensor responds to real irradiance changes even though the absolute calibration is loose.

* Plot to attach: BPW34 voltage time series (LOG009) showing the cloud transit. Source: `data-deployment/dana-raw/LOG009_fig1_timeseries.png`.

### Robot Design

**Platform.** Standard E80 submersible chassis reconfigured as a surface vehicle (dive components removed, hatches sealed). A vertical PVC mast carries the anemometer and weather vane about 0.5 m above the deck. The BPW34 photodiode is mounted horizontally on the deck, looking up.

**Center of gravity.** The mast offset was analyzed for metacentric stability and the boat stayed within the stable range for the planned wave conditions.

**Electrical integration.** Sensors interface through the MTB (Mudd Test Board), which carries the Teensy 4.0, regulator, motor driver, and J1 connector. Pin assignments:

* Pin 14: anemometer (digital interrupt)
* Pin 15 (A1): vane (analog)
* Pin 16 (A02): BPW34 (analog)
* Pins 18 and 19 (SDA, SCL): IMU (LSM303AGR addresses 0x19 and 0x1E)

**Software.** Cooperative scheduler at about 10 Hz (loop period 99 ms). Each sensor extends a `DataSource` pattern. Tasks are staggered by hardcoded timing offsets (Anemometer 85 ms, Weather Vane 90 ms) within each loop window to avoid I2C and SPI contention. Logging writes a CSV schema (`INFxxx.TXT`) and a binary log (`LOGxxx.BIN`) to SD card at 10 Hz.

**Navigation.** A closed-loop heading-hold P-controller and a station state machine (NAV_IDLE, NAV_DRIVE, NAV_STATION) were implemented and bench-tested but were not exercised on the water during the deployment. Deployment-day operation was open loop. The operator commanded motor pulses while GPS and sensors logged continuously.

* Photo to attach: deployment-day surface vehicle. Source: `data-deployment/robot_photo.png`.

### Deployment Details

**Site.** Dana Cove, a sheltered sub-bay of Dana Point Harbor, Orange County, California. Bounded west by the Dana Point headland, southwest by the Dana Point Pier, north by Baby Beach, and south by the open harbor basin (the cove's only entrance).

**Date and time.** 2026-04-25, approximately 09:30 to 11:00 PDT.

**Logs.** Three open-loop runs:

* LOG007 (7.3 min): initial deployment, sensor checkout.
* LOG008 (8.1 min): eastward transect.
* LOG009 (14.8 min): longest run, captured the cloud transit event.

**Transect geometry.** Maximum offshore distance reached was 145 m, measured westward from the easternmost GPS point. That eastern reference sits a few meters offshore of the beach line on the eastern side of the cove and is what we call "shore = 0" for distance-from-shore calculations.

**Launch and recovery.** Launched and recovered at the Westwind Sailing dock on the eastern shore.

**Safety precautions** (required by rubric):

* A spotter on the dock kept continuous visual contact and stayed in verbal communication with the operator.
* The vehicle was tethered for recovery, with a second tether and recovery boat on standby.
* We picked the cove over more open waters partly because boat traffic is light and predictable. Deployment was timed to avoid peak harbor traffic.
* Weather was monitored via WeatherSpark and visual observation. Conditions were calm with no precipitation, lightning, or rough surf.
* The platform is entirely passive: no discharges, no sediment disturbance, no significant underwater noise.

* Plot to attach: Dana Cove satellite overlay with the combined GPS track. Source: `data-deployment/Overlaid.png`.

### Results

These plots are the headline figures for the report. Each links to the source PNG generated by `analyze_dana.m` from the deployment CSV data.

1. **Combined GPS track on satellite imagery.** Establishes the deployment site as Dana Cove, a sheltered embayment. Source: `data-deployment/Overlaid.png`.
2. **BPW34 voltage vs distance from shore.** Shows the cloud-transit sensor-validation event at about 130 m. Source: `data-deployment/dana-raw/dana_figB_bpw_vs_dist.png`.
3. **Wind speed vs distance from shore.** Shows the inverse trend within the cove (2.77 m/s at 0 to 20 m, falling to 1.39 m/s at 120 to 140 m), consistent with progressively-deeper sheltering as the boat moved westward. Source: `data-deployment/dana-raw/dana_figC_wind_vs_dist.png`.
4. **Wind power density vs distance.** The cube law amplifies the wind-speed variation: a 2x speed range becomes an 8x power range. Source: `data-deployment/dana-raw/dana_figE_windpower_vs_dist.png`.
5. **Wind rose (count plus speed-weighted).** Demonstrates that the vane and IMU heading sensors are responsive and produce a clear directional concentration in the southern quadrant. Source: `data-deployment/dana-raw/dana_figF_windrose.png`.
6. **Energy resource comparison bar chart.** Three-way comparison: in-cove wind (6.2 W/m^2) vs open-coast wind (72.8 W/m^2) vs solar (475 W/m^2). Source: `data-deployment/energy_comparison.png`.

**Comparison to ground truth.**

* Wind speed: in-cove mean 2.16 m/s vs WeatherSpark coastal Dana Point 4.92 m/s. About a 2.3x ratio in speed and a 12x ratio in power density.
* Wind direction: in-cove dominant about 180 degrees (south) vs WeatherSpark regional about 215 degrees (south-southwest). About 35 degrees offset, consistent with magnetometer hard-iron error plus cove channeling.
* Solar irradiance: CIMIS Station 241 (San Clemente) reported 405 W/m^2 at 08:00 PST and 546 W/m^2 at 09:00 PST (which covers the 09:00 to 11:00 PDT deployment window). Mean 475 W/m^2.

### Conclusion

**Three- to four-sentence summary.** This project deployed a custom autonomous surface vehicle in Dana Cove on 2026-04-25 to measure wind and solar energy resources across a 145 m transect. The original nearshore-offshore gradient hypotheses could not be resolved at the available scale inside the harbor, but a clean three-way comparison of wind and solar energy density at the deployment site, at an open-coast reference, and at an inland reference is reported here. Solar irradiance was found to dominate wind power density by 77x at the deployment site, and the cove sheltering effect alone reduced wind power density by an order of magnitude relative to the open coast. For nearshore-bay environments at this scale, photovoltaic generation is the demonstrably more available resource by nearly two orders of magnitude.

**Big-picture takeaways.**

* A nearshore site behind a breakwater or headland delivers about an order of magnitude less wind power density than its open-coast neighbor a few hundred meters away.
* Solar dominates wind in this microclimate by 77x, and even compared to open-coast wind by 6.5x.
* The cube law (P/A scales as v^3) means small wind-speed differences become large power differences, so site-specific measurement matters for nearshore renewable siting.

**Recommendations for future work and what we would change next time.**

* Extend the transect outside the breakwater to a 500 m to 1 km open-coast deployment to capture the original gradient hypothesis cleanly.
* Calibrate the magnetometer (figure-eight cal) to remove the hard-iron offset and tighten the wind direction estimate by 15 to 30 degrees.
* Run a multi-point vane mechanical calibration through known angles to validate the linear voltage-to-angle map.
* Recover or replace the VEML6030 ambient light sensor, which did not enumerate on the I2C bus during deployment despite power being present.
* Water-test the closed-loop heading-hold controller and station state machine that we implemented and bench-tested but never ran on the water.

### Acknowledgements

**Team 32 contributions.**

* **Samara Baidwan**: overall project management, software, and integration across the robot.
* **Felix Peng**: wrote most of the robot software, designed and built both photodiode subsystems (BPW34 plus TIA, and the VEML6030 integration), and built the MATLAB analysis pipeline.
* **Ava Barasch**: mechanical design and structural integration of the surface vehicle.
* **Andy Maldonado**: weather vane and anemometer mechanical and sensor assemblies.

**Other thanks.**

* HMC E80 instructional staff for guidance throughout the project.
* Westwind Sailing for launch and recovery access at Dana Cove.

### References

To format as IEEE-style numbered citations in the final report:

1. NOAA National Data Buoy Center, "How does the wind speed over the ocean compare with wind speed over the land?" NDBC Education Resources. https://www.ndbc.noaa.gov/education/windspeed_ans.shtml
2. NASA, "Albedo Values," MyNASAData. https://mynasadata.larc.nasa.gov/basic-page/albedo-values
3. S. Maritorena, A. Morel, and B. Gentili, "Diffuse reflectance of oceanic shallow waters: Influence of water depth and bottom albedo," Limnology and Oceanography, vol. 39, no. 7, pp. 1689 to 1703, 1994.
4. WeatherSpark, "Historical Weather on Saturday, April 25, 2026 in Dana Point, California, United States." https://weatherspark.com/h/d/1838/2026/4/25/Historical-Weather-on-Saturday-April-25-2026-in-Dana-Point-California-United-States
5. California Irrigation Management Information System (CIMIS) Station 241, San Clemente, South Coast Valleys, California Department of Water Resources. https://cimis.water.ca.gov/
6. Vishay Semiconductors, BPW34 Silicon PIN Photodiode datasheet. https://www.vishay.com/docs/81521/bpw34.pdf
7. Diodes Incorporated, AH9246 Omnipolar Hall-Effect Sensor datasheet. https://www.diodes.com/assets/Datasheets/AH9246.pdf
8. Melexis, MLX90316 Programmable Rotary Position Sensor datasheet. https://www.melexis.com/-/media/files/documents/datasheets/mlx90316-datasheet-melexis.pdf
9. STMicroelectronics, LSM303AGR e-Compass IMU datasheet. https://www.st.com/resource/en/datasheet/lsm303agr.pdf

## Image and Figure Inventory

When we move this to the Google Doc, here are the images that need to be inserted, with their target sections.

| # | Caption | Path | Status |
|---|---------|------|--------|
| 1 | Sensor Design: weather vane wind rose (count plus speed-weighted) | `data-deployment/dana-raw/dana_figF_windrose.png` | ready |
| 2 | Sensor Design: BPW34 voltage time series with cloud event | `data-deployment/dana-raw/LOG009_fig1_timeseries.png` | ready |
| 3 | Robot Design: deployment-day surface vehicle photo | `data-deployment/robot_photo.png` | ready |
| 4 | Deployment Details: Dana Cove satellite overlay with GPS track | `data-deployment/Overlaid.png` | ready |
| 5 | Results: combined GPS track | `data-deployment/dana-raw/dana_figA_gps_track.png` | ready |
| 6 | Results: BPW34 voltage vs distance from shore | `data-deployment/dana-raw/dana_figB_bpw_vs_dist.png` | ready |
| 7 | Results: wind speed vs distance from shore | `data-deployment/dana-raw/dana_figC_wind_vs_dist.png` | ready |
| 8 | Results: wind power density vs distance from shore | `data-deployment/dana-raw/dana_figE_windpower_vs_dist.png` | ready |
| 9 | Results: energy resource comparison bar chart (3-way) | `data-deployment/energy_comparison.png` | ready |
