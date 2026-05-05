# Final Report Worksheet, Team 32 Casual Draft

## Abstract

We built a small autonomous surface vehicle that goes out on the water and measures wind speed, wind direction, and sunlight as it moves. We took it to Dana Cove inside Dana Point Harbor and ran it across about 145 meters of pretty sheltered water. Because the cove is small and tucked behind a breakwater, we couldn't really test our original idea about how wind and sunlight change as you go further from shore at full coastal scale. So we shifted the project into more of a comparison of how much wind and solar energy is actually available at our spot versus an open coast reference and a nearby inland reference.

For the headline numbers, inside the cove we measured an average wind power density of about 6.2 W/m^2, with average wind speeds around 2.16 m/s. The open coast version of Dana Point during the same hours had wind speeds closer to 4.92 m/s, which works out to about 72.8 W/m^2. Solar irradiance over our deployment window was about 475 W/m^2 according to a nearby CIMIS station. That means solar beat our measured wind by roughly 77 times, beat open coast wind by 6.5 times, and the cove sheltering alone reduced wind power by about 12 times compared to the open coast.

## Introduction

Coastal wind is supposed to pick up speed as you move from rough nearshore water to smoother open water, since drag drops off when the surface gets smoother. The reflectance of sunlight off the surface also changes with depth and bottom type, with shallow sandy water reflecting more than deep water. Wind power density follows a cube law (P/A = 0.5 x ρ x v^3), so even small changes in wind speed turn into big changes in available power. Local features like breakwaters, piers, and headlands also knock the wind down a lot, which is why energy maps based on satellite or coarse models don't always match what you actually measure at a specific site. Our sensors for the project are all pretty standard for this kind of work: a cup anemometer for wind speed, a vane on a contactless rotary sensor for wind direction, a photodiode for sunlight, GPS for position, and an IMU for compass heading.

## Sensor Design

We carried four sensing systems plus GPS. The anemometer measures wind speed, the vane measures wind direction, the photodiode measures sunlight, and the IMU gives us our compass heading so we know which way the boat itself is facing at any given moment.

The anemometer is a 3D printed cup rotor with a small magnet, spinning past a hall effect chip. The chip sends one pulse per revolution to the microcontroller, and we count those pulses to get an RPM, then convert that to a wind speed using a calibration we did in the wind tunnel back at HMC. The range we cared about was roughly zero up to 10 m/s for a typical sea breeze, and on deployment day we saw wind speeds between zero and around 4 m/s. The signal was clean for all three runs, so as a field check the anemometer worked just fine.

The vane is also a 3D printed shape that turns a magnet over a contactless rotary position sensor. The sensor outputs a voltage that maps linearly to angle from 0 to 360 degrees. We measured the two endpoints of that voltage range on the bench before going out, and we also measured what voltage the vane shows when it's pointed straight forward along the bow, so we could subtract that out and get an angle relative to the boat. Then we add the IMU heading to convert that into a real world compass direction. When we plotted a wind rose from all the deployment data, the wind clearly came from the south, only off by about 35 degrees from what WeatherSpark reported for the area, and that residual offset is what we'd expect from an uncalibrated magnetometer plus the cove channeling the wind a bit.

The BPW is a small silicon photodiode that puts out a tiny current proportional to how much light hits it. We feed that current into a transimpedance amplifier, which is just an op amp with a big feedback resistor that turns the current into a voltage we can read with the microcontroller's ADC. We did a rough lab calibration linking voltage to irradiance, but it's not super tightly bounded. On deployment day the voltage sat around 0.52 V most of the time and dropped to about 0.42 V when a cloud passed overhead during the longest run, which is a clean 25 percent reduction. So even though the absolute calibration is loose, the relative response of the BPW is clearly real and the sensor noticed the cloud event without any prompting.

## Robot Design

The robot is a standard E80 submersible chassis that we converted into a surface vehicle by sealing up the dive components and adding a vertical PVC mast on top. The anemometer and vane sit on the mast about half a meter above the deck, where they can actually see the wind without the boat itself getting in the way. The photodiode is mounted flat on the deck looking straight up at the sky. The microcontroller, motor driver, and other electronics live inside the watertight enclosure. The software runs a cooperative scheduler at about 10 Hz, so every sensor gets read 10 times per second and everything gets logged to the SD card. We had a closed loop heading hold controller and a station based mission state machine ready to go, but on deployment day we operated the boat open loop, with the operator manually triggering motor pulses while the GPS and the sensors logged in the background.

## Deployment Details

We deployed in Dana Cove, which is the small sheltered sub bay tucked into Dana Point Harbor. It's bounded by the Dana Point headland on the west, the pier on the southwest, Baby Beach on the north, and the open harbor basin on the south, which is also the cove's only opening to water beyond the breakwater. We launched and recovered from the Westwind Sailing dock on the east side of the cove. The deployment was on April 25th, 2026, from about 9:30 to 11 in the morning local time. We did three open loop runs, the first being a short shake out, the second a transect heading east and back, and the third a longer one that picked up a cloud passing overhead and a wider variety of motion. The maximum offshore distance we ever reached was 145 meters, measured westward from our easternmost GPS point, which itself sits a few meters off the beach line.

For safety, one of us was always on the dock keeping eyes on the boat and in voice contact with whoever had the controller. The boat was tethered for recovery the whole time. The cove has light and predictable boat traffic, so we didn't have to dodge much. The weather was calm with no precipitation or rough surf. And the platform itself is passive: it doesn't release anything into the water or make any meaningful underwater noise.

## Results

The headline finding from the deployment is the three way energy comparison. Solar dominates wind in this microclimate by about 77 to one. Even compared to the open coast version of Dana Point reported by WeatherSpark, solar still beats wind by about 6.5 to one. And the cove itself drops wind power density by about a factor of 12 compared to the open coast just a few hundred meters away.

Within the cove itself, we did see a soft inverse trend in wind speed versus distance from shore. Speeds were roughly 2.77 m/s near the eastern shore and dropped to 1.39 m/s at the offshore end of the transect. The cube law turns that 2 to 1 speed range into about an 8 to 1 power range, which is a real gradient even though its shape is the opposite of what we originally expected. We thought wind would pick up offshore, but it dropped, and the most likely explanation is that the boat was moving deeper into the wind shadow of the headland and the breakwater as it went westward. So increasing distance from shore was actually correlated with increasing shelter rather than less.

The BPW signal stayed mostly flat across all three runs, with the cloud transit during the longest run being the cleanest sensor validation event of the day. The wind rose pointed clearly south at about 180 degrees, while the regional report had the wind coming from about 215 degrees. The 35 degree offset is consistent with magnetometer hard iron error plus a little local channeling from the cove geometry, and since we never did a figure eight magnetometer calibration before deploying, that's exactly the kind of residual error we'd expect to be left over.

## Conclusion

We built and deployed a working autonomous surface vehicle that captured useful wind and solar data at a real coastal site over about 30 minutes of total deployment time. We didn't get to test the original gradient hypothesis at full coastal scale because the cove is tighter than what we'd originally planned for, but we did get a clean three way comparison of energy resources that has a clear takeaway. In a sheltered nearshore site like this one, solar is by far the more available resource. Wind power density is reduced by an order of magnitude inside the breakwater compared to the open coast, and even the open coast version of Dana Point sits well below the solar resource.

Looking forward, the obvious next step is to extend the transect outside the breakwater to a kilometer scale offshore deployment, so we can capture the original nearshore to offshore gradient cleanly. We'd also calibrate the magnetometer beforehand with a figure eight, do a more thorough multi point calibration of the vane against known angles, and finally water test the closed loop heading controller and the station based mission state machine that were sitting on the bench for this round.

## Acknowledgements

Samara handled overall project management and software integration across the robot. Felix wrote most of the robot software, designed and built both photodiode subsystems, and built the MATLAB analysis pipeline. Ava handled the mechanical design and structural integration of the surface vehicle. Andy built the weather vane and anemometer mechanical assemblies. Big thanks to the E80 instructional staff for guidance throughout the semester, and to Westwind Sailing for letting us launch and recover from their dock at Dana Cove.

## References

1. NOAA National Data Buoy Center, "How does the wind speed over the ocean compare with wind speed over the land?" https://www.ndbc.noaa.gov/education/windspeed_ans.shtml
2. NASA, "Albedo Values," MyNASAData. https://mynasadata.larc.nasa.gov/basic-page/albedo-values
3. S. Maritorena, A. Morel, and B. Gentili, "Diffuse reflectance of oceanic shallow waters: Influence of water depth and bottom albedo," Limnology and Oceanography, vol. 39, no. 7, 1994.
4. WeatherSpark, "Historical Weather on Saturday, April 25, 2026 in Dana Point, California, United States." https://weatherspark.com/h/d/1838/2026/4/25/Historical-Weather-on-Saturday-April-25-2026-in-Dana-Point-California-United-States
5. California Irrigation Management Information System (CIMIS) Station 241, San Clemente. https://cimis.water.ca.gov/
6. Vishay Semiconductors, BPW34 Silicon PIN Photodiode datasheet. https://www.vishay.com/docs/81521/bpw34.pdf
7. Diodes Incorporated, AH9246 Omnipolar Hall-Effect Sensor datasheet. https://www.diodes.com/assets/Datasheets/AH9246.pdf
8. Melexis, MLX90316 Programmable Rotary Position Sensor datasheet. https://www.melexis.com/-/media/files/documents/datasheets/mlx90316-datasheet-melexis.pdf
9. STMicroelectronics, LSM303AGR e-Compass IMU datasheet. https://www.st.com/resource/en/datasheet/lsm303agr.pdf
