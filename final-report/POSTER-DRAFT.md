# E80 Team 32 Poster Draft

This file mirrors the section structure of `report-templates/E80_PosterTemplate.pptx` so you can copy each block straight into the corresponding text box. I cannot directly edit the .pptx file, so this is a paste-friendly intermediate.

A note before you start: the template literally says "Autonomous Underwater Vehicle (AUV) Features." Our project is a surface vehicle, not an AUV. Rename that section to "Autonomous Surface Vehicle (ASV) Features" in PowerPoint.

## Title block

Quantifying Wind and Solar Energy Resources in a Sheltered Coastal Embayment Using an Autonomous Surface Vehicle

Team 32: Samara Baidwan, Felix Peng, Ava Barasch, Andy Maldonado

E80 Experimental Engineering, Spring 2026

## Problem Statement

Coastal renewable energy siting depends on knowing how wind and solar resources change over short distances near the shore. Models and regional weather stations smooth over local terrain effects like breakwaters, piers, and headlands, which can attenuate wind dramatically over just a few hundred meters. We built an autonomous surface vehicle to take direct measurements of wind speed, wind direction, and solar irradiance at a real coastal site, comparing what's actually available to what regional sources predict.

## Impact

Site-specific wind and solar measurements give renewable installers data that satellite models miss. A nearshore site behind a breakwater or headland can deliver an order of magnitude less wind power than its open-coast neighbor a few hundred meters away. Knowing this matters for siting decisions on coastal monitoring buoys, navigation aids, and hybrid wind/solar coastal installations.

## Autonomous Surface Vehicle (ASV) Features

Standard E80 submersible chassis converted to a surface platform by sealing the dive components and adding a vertical PVC mast. The mast carries the cup anemometer and weather vane about 0.5 m above the deck so they see the wind without the hull blocking it. The photodiode mounts flat on the deck, looking up at the sky. A Teensy microcontroller runs a cooperative scheduler at 10 Hz, reading and logging every sensor to an SD card.

A closed loop heading hold controller and a station based mission state machine were implemented and bench tested. On deployment day we operated open loop, with manual motor pulses while the GPS and sensors logged continuously.

## Mechanical Design & Integration

[Insert: robot photo or CAD render. Suggested file: data-deployment/robot_photo.png (placeholder, Felix to provide deployment-day photo)]

The chassis is the standard E80 submersible body, modified for surface operation. The mast is vertical PVC, sized to keep the wind sensors clear of the hull's airflow disturbance. Center of gravity offset from the mast was checked for metacentric stability under expected wave conditions and stayed in the stable range.

## Electrical Schematic & Sensors

[Insert: MTB or sensor wiring schematic. Reference: sensor-references/MTB_Schematic_3.0.2021.pdf]

Sensors interface through the MTB (Mudd Test Board), which carries the Teensy 4.0, regulator, motor driver, and the J1 connector. The cup anemometer is a digital pulse input on Teensy pin 14. The weather vane and the photodiode TIA are analog inputs on pins 15 and 16. The IMU sits on the I2C bus on pins 18 and 19.

## Sensor Calibration

Anemometer: wind tunnel calibrated at HMC pre-deployment. Linear fit between RPM and reference wind speed gave a slope of 21.1 and intercept of 1.71, which converts pulse counts to m/s.

Weather vane: voltage endpoints (0.094 V and 3.000 V) measured pre-deployment by sweeping through one full revolution. Mechanical zero (vane pointed along the bow) measured at 0.52 V, which corresponds to a raw angle of 52.8 degrees and is subtracted from each sample to give wind direction relative to the boat. The IMU heading is then added to convert to compass direction.

Photodiode and TIA: rough lab calibration linking photodiode voltage to known illumination. Field cross-validation against a CIMIS reference station confirms the relative response, though the absolute conversion is loose.

## Deployment Details

Location: Dana Cove, a sheltered sub-bay of Dana Point Harbor in Orange County, California. The cove is bounded west by the Dana Point headland, southwest by the Dana Point Pier, north by Baby Beach, and south by the open harbor basin. Launched and recovered from the Westwind Sailing dock on the east side. Maximum offshore distance reached was 145 m.

[Insert: site overlay. Suggested file: data-deployment/Overlaid.png]

Testing Protocol: three open-loop runs on 2026-04-25 between 09:30 and 11:00 PDT. LOG007 was a 7.3-minute shakedown. LOG008 was an 8.1-minute east-west transect. LOG009 was the longest run at 14.8 minutes and captured a brief cloud transit event. Spotter on the dock kept continuous visual contact and stayed in voice contact with the operator. The boat was tethered the whole time for recovery.

## Ground Truth Measurements

Wind speed and direction: WeatherSpark historical record for Dana Point, 09:00 to 11:00 PDT on 2026-04-25, reported wind speed of about 4.92 m/s (11 mph) from 215 degrees (south-southwest).

Solar irradiance: California Irrigation Management Information System (CIMIS) Station 241 in San Clemente, the closest station to Dana Point. Reported 405 W/m^2 at 08:00 PST and 546 W/m^2 at 09:00 PST, covering the deployment window. Mean of about 475 W/m^2.

## Experimental Data and Results

Headline numbers (largest type on the poster):

77x: solar dominance over in-cove wind
6.5x: solar dominance over open-coast wind
11.7x: wind power reduction inside the cove versus open coast

[Insert plot: data-deployment/dana-raw/dana_figC_wind_vs_dist.png — wind speed versus distance from shore, showing the inverse trend within the cove]

[Insert plot: data-deployment/dana-raw/dana_figF_windrose.png — wind rose showing the dominant southern direction]

[Insert plot: data-deployment/dana-raw/dana_figE_windpower_vs_dist.png — wind power density versus distance from shore]

[Insert plot: data-deployment/energy_comparison.png — three-way bar chart comparing in-cove wind, open-coast wind, and solar irradiance]

## Results & Conclusions

In Dana Cove on the morning of 2026-04-25, solar irradiance dominated wind power density by 77 times at mast height (475 W/m^2 versus 6.2 W/m^2). Compared with a contemporaneous open-coast wind reference, solar still exceeded wind by 6.5 times. The cove sheltering effect alone reduced wind power density by 11.7 times relative to the open coast.

Within the cove, wind speed decreased with distance from the launch shore, the opposite of what we originally hypothesized. The cause is that the boat moved deeper into the wind shadow of the headland and breakwater as it went westward, so increasing distance from shore correlated with increasing shelter rather than openness. The cube law (P/A scales as v^3) turned a 2-to-1 speed range into an 8-to-1 power range across the transect.

For nearshore-bay environments at this scale, photovoltaic generation is the demonstrably more available resource by nearly two orders of magnitude.

## Future Work

Extend the transect outside the breakwater to a 500 m to 1 km open-coast deployment to capture the original nearshore-to-offshore gradient cleanly.

Calibrate the magnetometer with a figure-eight sweep before deployment to remove hard-iron offset and tighten the wind direction estimate by 15 to 30 degrees.

Multi-point vane calibration through known angles to validate the linear voltage-to-angle map.

Recover or replace the VEML6030 ambient light sensor that did not enumerate on the I2C bus during deployment despite power being present.

Water-test the closed loop heading controller and the station based mission state machine that were implemented and bench-tested but never run on the water.

## Acknowledgements

Thanks to the E80 instructional staff for guidance throughout the project, and to Westwind Sailing for letting us launch and recover from their dock at Dana Cove.

Team 32: Samara (project management and software integration), Felix (robot software, photodiode subsystems, MATLAB analysis pipeline), Ava (mechanical design and structural integration), Andy (weather vane and anemometer assemblies).
