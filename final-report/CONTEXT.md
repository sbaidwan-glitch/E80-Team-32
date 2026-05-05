# CONTEXT.md: Final Writeup Working Notes

Living document. Source of truth for what we know, what's broken, what's missing, and what needs to be honest in the report. Update as we go.


## Project recap (from one-page draft 2)

Title: Nearshore-to-Offshore Renewable Energy Resource Assessment Using an Autonomous Surface Vehicle.

Two hypotheses, both vs. distance from shore (independent variable, GPS-derived):
1. Wind speed increases with distance from shore.
2. Solar irradiance decreases with distance from shore (sand albedo + shallow-bottom reflectance fall off).

Sensors deployed:
- Anemometer: 3D-printed cup + AH9246 hall sensor (revolution counting via digital interrupt).
- Weather vane: 3D-printed vane + MLX90316 contactless rotary position sensor.
- Solar irradiance: VEML6030 (lux) **and** BPW34 photodiode (TIA voltage).
- Air temperature: MCP9701A (for air-density correction in P/A = 0.5*ρ*v^3).


## Deployment status: Dana Point (2026-04-25, 09:00-11:00)

Data fully ingested 2026-04-26.

### What we have (Dana Point)

Three logs, all `DanaPointOpen` sketch (no `DanaPointClosed` was ever run on deployment day: important):

| LOG | Duration | GPS lock | Mag quality | Notes |
|-----|----------|----------|-------------|-------|
| LOG007 | 7.3 min | yes | center (2.9, -0.5) µT, r=24.5 µT: clean | best mag of the three |
| LOG008 | 8.1 min | yes | center (13.5, -12.1) µT, r=19.7 µT: hard-iron offset | mag offset flagged |
| LOG009 | 14.8 min | yes | center (13.4, 3.6) µT, r=24.8 µT: hard-iron offset | longest run, BPW shows clear shadow event |

Total: ~30 min of usable data with GPS, BPW34 voltage, vane, IMU heading, anemometer.

### Phake Lake (validation / fallback)

| LOG | Duration | GPS lock | Notes |
|-----|----------|----------|-------|
| LOG019 | 2.2 min | no | older sketch, no A02/anemo columns: partial data only |
| LOG020 | 0.5 min | yes | very short; mag heavily offset |
| LOG021 | 6.8 min | yes | usable as sensor-validation appendix |

### What worked
- GPS logged throughout all three Dana logs (lat/lon + nsats).
- BPW34 photodiode logged voltage on A02 throughout, with a clear ~0.55 V → ~0.40 V step in LOG009 around minute 12 (cloud / shadow event: useful for the Results section).
- Anemometer pulse counting active across all Dana logs.
- Mechanical platform survived 30+ min on the water with mast-mounted sensors.

### What broke / what's noisy
- **VEML6030:** not in the logs. We're not investigating further. Brief mention only in the report.
- **VEML lux trace in figure 1 panel 2** shows a flat line at ~1.1x10⁵: that's not real data, that's the NaN-default panel rendering against the saturated y-axis. Ignore: VEML had no samples.
- **Magnetometer:** noticeable hard-iron offset in LOG008/LOG009 (centers of (13.5, -12.1) and (13.4, 3.6) µT respectively). LOG007 is clean (center near origin). Mean radius is ~20-25 µT: Earth field is 25-65 µT, so we're slightly compressed but not saturated. Effect on report:
  - IMU heading is noisy / wrap-jumpy, especially at compass-north crossings.
  - True wind direction (`vane_angle + headingIMU mod 360`) inherits that noise.
  - Mitigation: report wind direction *modulo aggregating over 10-30 s windows* and acknowledge the compass calibration limit.
- **Vane angle trace:** wraps wildly between 0 degrees and 360 degrees (wind direction near the 0/360 boundary). Standard polar-data issue. Plot as wind rose / unwrap before averaging.

### BPW34 cross-calibration plan (with new ground truth)
- **Ground truth ready:** CIMIS Station 241 (San Clemente) hourly Solar Rad in Langleys/hr → multiply by 11.63 to get W/m^2.
- **Procedure:**
  1. From each Dana CSV, take the BPW voltage sample(s) closest to each top-of-hour (09:00, 10:00, 11:00 PDT).
  2. Plot BPW voltage (x) vs. CIMIS irradiance (y) for those points.
  3. Linear fit → field-derived calibration `irradiance = m*V + b`.
  4. Overlay the original lab cal line. Discuss agreement / disagreement.
- **Caveat to call out in the report:** the BPW34 sees mostly direct-down sky+sun, the CIMIS pyranometer sees the same on land. They should agree under clear sky on the deployment day. The cloud event in LOG009 (~min 12) where BPW drops from 0.55 V → 0.40 V is itself a useful sanity check: it proves the BPW responds to real irradiance changes.

### Navigation: open loop only on deployment day: correction
- All three Dana logs auto-detected as `DanaPointOpen`. **No `DanaPointClosed` was actually run on deployment day.**
- Earlier I wrote "both closed-loop and open-loop were exercised" based on Felix's verbal recap, but the data contradicts that. The closed-loop sketch (with `NAV_DRIVE` state machine + heading-hold P-controller + station detection) wasn't run on the water on 2026-04-25.
- **What we *did* have on deployment day:**
  - Open-loop motor control (operator pulses on/off as needed: visible in LOG009 motor-C trace).
  - GPS logging.
  - All sensors logging at 10 Hz.
- **What we *built* but didn't run on deployment day:**
  - `DanaPointClosed` sketch with state machine (`NAV_IDLE` → `NAV_DRIVE` → `NAV_STATION`).
  - Heading-hold P-controller against IMU.
  - Auto station detection on motor-off.
- **Honest framing confirmed by Felix 2026-04-27:** "An autonomous closed-loop heading-hold controller and station state machine were implemented and bench-tested. On deployment day, scope was reduced to operator-driven open-loop control with continuous GPS and sensor logging, prioritizing sensor data collection over autonomous navigation. The closed-loop infrastructure is documented in the codebase as future work."
- All Phake Lake logs (019/020/021) are also `DanaPointOpen`. Closed-loop code was bench-tested but never water-tested. Report this honestly.

### Distance from shore: definition (confirmed by Felix 2026-04-27)

Shore reference for the `distance_from_shore_m` derived column = **easternmost GPS point in each track**, which Felix confirms is "right at the beach line, a few meters from where sand and waves meet, but as early as we can start." All other points are westward (further from shore = larger distance value). 2D Euclidean distance in meters using the equirectangular approximation (small area, <500 m, error negligible).

Caveat for the report: the GPS track is a wandering 2D path, not a strict shore-perpendicular transect. Distance-from-shore is therefore a derived 1D coordinate that compresses some N-S motion into the offshore axis. Acknowledge this in the Results & Deployment Details section.


## Phake Lake fallback

We also have a Phake Lake test deployment. Raw data in `data-deployment/phake-raw/`. If Dana Point data is too thin to support both hypotheses, we lean on Phake Lake for:
- Sensor functionality validation (anemometer, vane, BPW responding correctly in a controlled environment).
- Possibly wind/light variation across the lake (less compelling than the ocean nearshore-to-offshore story, but real data).

The report's primary story stays Dana Point. Phake Lake is sensor validation / appendix material.


## Ground-truth data sources (collected 2026-04-26)

All three live as PNGs at the root of `data-deployment/`.

**Solar irradiance: `radiation.png`:**
- Source: **CIMIS Station 241: San Clemente: South Coast Valleys** (very close to Dana Point: much better than Irvine/Camp Pendleton).
- Hourly table for 2026-04-25 with: ETo, Solar Rad (Ly/hr), vapor pressure, air temp ( degreesF), RH, dew point, wind speed (mph), wind direction, soil temp.
- **Solar Rad column is in Langleys/hour. Convert to W/m^2: `W/m^2 = Ly/hr x 11.63`.** (1 Langley = 1 cal/cm^2; 1 Ly/hr = 41,840 J/m^2/hr ÷ 3600 s/hr = 11.63 W/m^2.)
- Daily totals row at bottom: ~625 Ly/day total → ~7,266 Wh/m^2 for the day. Sensible for a clear April day in SoCal.
- This is our primary irradiance ground truth for the BPW34 cross-calibration.

**Wind speed: `Wind Speed on Saturday, April 25, 2026 in Dana Point.png`:**
- Source: **WeatherSpark, historical weather page for Dana Point, 2026-04-25.** URL: https://weatherspark.com/h/d/1838/2026/4/25/Historical-Weather-on-Saturday-April-25-2026-in-Dana-Point-California-United-States
- Hourly Dana Point wind speed (mph) over the full day.
- 09:00-11:00 deployment window: ~10-12 mph, gusts logged later in the afternoon (peak 26.5 mph at 12:53 PM).

**Wind direction: `Wind Direction on Saturday, April 25, 2026 in Dana Point.png`:**
- Source: same WeatherSpark page as wind speed.
- Hourly. 09:00-11:00 window: roughly 200-230 degrees (SSW to WSW). Wind blowing FROM the south/southwest.

These three together are enough ground truth for the Results section.


## Answered (confirmed 2026-04-26)

- **Navigation:** both heading-hold (closed loop) AND open-loop segments were exercised on deployment day. Use honest dual framing in Robot Design section.
- **Air temp / thermistor:** scrapped entirely. Was always a backup. Drop the air-density correction or note we're omitting it; do not include MCP9701A in the sensor list.
- **Anemometer calibration:** done. Lives in `SensorAnemometer.h`: `velocity = (rpm + 1.71) / 21.1` (`A_CAL = 21.1`, `B_CAL = -1.71`). Need the raw cal data / I-O files for the report's Sensor Validation subsection: Felix to confirm location.
- **BPW34 calibration plan:** the original lab cal was rough. Build a *new* cal from deployment-day BPW voltage vs. CIMIS / NREL ground-truth irradiance. Compare to the original line of best fit. Both go in the report.
- **VEML failure mode (deployment day):** power was on, sensor was not being recognized, no data collected. We're not investigating further. The report just notes that the VEML stopped working and "needs to be checked up on": keep it brief, don't dedicate a paragraph. BPW34 carries the optical story.
- **Source of truth for code details:** the actual code wins over comments and CLAUDE.md. Stale comments are a known fact of the repo; verify against `.h`/`.cpp` or the canonical `logreader_danapoint.m` column comments. Pin numbering questions are not worth chasing for the report: the log columns are correctly labeled in the canonical reader, which is what the analysis depends on.

## Still open: please answer when you can

1. **Station count and spacing:** How many stations did you log at on 2026-04-25? How long did you dwell at each? Were they roughly along a shore-perpendicular transect?
2. **Anemometer cal data location:** Where are the wind-tunnel raw input/output files? (RPM at known wind speeds.) I want to plot the cal curve for the Sensor Validation section.
3. **GPS quality:** Any dropouts at the launch point (close to cliffs/buildings/people)? `nsats` time series in MATLAB will tell us: easy to read off the auto-saved CSV once you run the logreader.
4. **Heading-hold quality:** While `NAV_DRIVE` was running, was the robot tracking the commanded heading well? A commanded-vs-actual heading plot (figure 4 from the logreader) would be a strong Robot Design figure.
5. **Solar irradiance source:** Felix is searching for a closer-to-Dana-Point source than CIMIS. Update CONTEXT.md "Ground-truth data sources" section once chosen.


## Rubric requirements we need to address (from `deliverables-guidance/E80 Project Final Deliverables`)

Sections expected in the report:
1. **Abstract**: quantitative key result (e.g., "BPW34 voltage decreased X% from station 1 to station N over Y meters offshore").
2. **Introduction**: science/engineering goal.
3. **Sensor Design** (one full page per sensor, ~4 sensors): theory, selection, circuit schematic, software (10 Hz adequate?), validation/calibration.
4. **Robot Design**: mechanical calcs (CG, mast loading, ballast?), sensor mounting, waterproofing, regulator schematics.
5. **Results & Deployment Details**: pre-deployment prep, deployment site/trajectory, **safety precautions** (mandatory).
6. **Results**: comparisons to theory and ground truth.
7. **Conclusion**: recap, future work, takeaway.

Hard constraint: **10-page max**, IEEE format, individual report (one per person, full team in acknowledgments).

Due dates:
- **Poster:** 2026-05-01 noon (uploaded), presented 2026-05-06.
- **Report:** check Final Report Worksheet PDF for exact deadline.


## File map

```
final-writeup/
├── CONTEXT.md                ← this file
├── deliverables-guidance/    ← instructor PDFs (rubrics, deployment rules, checklists)
├── project-description/      ← original one-pagers and proposal
├── sensor-references/        ← datasheets, MTB schematic, calibration notes
├── report-templates/         ← IEEE LaTeX template + worksheet/rubric/peer-review/poster PPTX
├── example-report/           ← Fernandez/Norfleet/Rohde prior-year report
└── data-deployment/
    ├── matlab_outputs.md     ← paste logreader_danapoint.m console output here
    ├── dana-raw/             ← Dana Point BIN/INF/CSV/figures
    └── phake-raw/            ← Phake Lake BIN/INF/CSV/figures (fallback)
```
