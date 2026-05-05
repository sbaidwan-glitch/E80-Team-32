# MATLAB Logreader — Run Notes

`logreader_danapoint.m` now auto-exports everything per run. **You don't need to paste console output or screenshot figures anymore.** Each run drops files directly into `dana-raw/` (or `phake-raw/` if you set `deployment = 'phake'`):

| File | What it is |
|---|---|
| `LOG<nnn>_console.txt` | Full MATLAB console output (`diary` capture) |
| `LOG<nnn>.csv` | Every logged column + derived voltages + `wind_dir_true_deg` + `station_number` |
| `LOG<nnn>_fig1_timeseries.png` | 6-panel sensor time series (wind, lux, BPW, vane, heading, motors) |
| `LOG<nnn>_fig2_gps.png` | GPS track with station stops marked (only if GPS lock) |
| `LOG<nnn>_fig3_mag.png` | Magnetometer scatter + axes (only if mag data) |
| `LOG<nnn>_fig4_controller.png` | Heading-hold controller state (DanaPointClosedShort only) |
| `LOG<nnn>_fig5_anemo.png` | Anemometer pulse count (only if `anemo_pulses` column) |

## How to use this file

This document is for the *qualitative context* only — what each LOG was for, conditions on the day, what's notable. The numbers come from the auto-saved files.

For each run you want to include in the report, add a section below.

---

## Dana Point — LOG###

- **What this run was for:** (e.g. "first transect, station 1→3, closed-loop heading hold")
- **Sketch variant:** DanaPointClosed / DanaPointOpen / DanaPointClosedShort
- **Time of day:** ~__:__ to __:__ PDT
- **Conditions:** sky / wind / sea state / anything notable
- **Anything weird:** (mid-run failures, tether snags, operator interventions)

---

## Dana Point — LOG###

- **What this run was for:**
- **Sketch variant:**
- **Time of day:**
- **Conditions:**
- **Anything weird:**

---

## Phake Lake — LOG###

- **What this run was for:** (sensor validation / wind-and-light variation across the lake)
- **Sketch variant:**
- **Time of day:**
- **Conditions:**
- **Anything weird:**
