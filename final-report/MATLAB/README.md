# MATLAB Analysis Bundle - Team 32 Dana Cove Deployment

Self-contained bundle for reproducing every plot in the report from the
deployment-day data.

## What's in here

```
MATLAB/
  analyze_dana.m          - Multi-log analysis: GPS track, distance plots,
                            wind rose, energy comparison.
  logreader_project.m     - Single-log inspection: each sensor as its own
                            full-size figure plus a GPS track.
  logreader_danapoint.m   - Reads raw SD-card binary logs (LOG###.BIN +
                            INF###.TXT) and writes a CSV per log. Only
                            needed if you have new BIN files from the
                            deployment SD card; the CSVs from our day are
                            already in dana-raw/.
  dana-raw/
    LOG007.csv            - 7.3 min shake-out log
    LOG008.csv            - 8.1 min offshore-path log
    LOG009.csv            - 14.8 min long log (with cloud transit event)
    dana_combined.csv     - All three concatenated, with derived columns
                            (distance from shore, wind power density,
                            wind direction in earth frame).
```

## How to reproduce all the plots

1. Open MATLAB (R2020a or newer).
2. `cd` into this `MATLAB/` folder.
3. Run `analyze_dana`.

The script writes seven PNGs into `dana-raw/`:

- `dana_figA_gps_track.png`        - Combined GPS track of all 3 logs
- `dana_figB_bpw_vs_dist.png`      - BPW34 voltage vs distance from shore
- `dana_figC_wind_vs_dist.png`     - Wind speed vs distance from shore
- `dana_figD_winddir_vs_dist.png`  - Wind direction vs distance (both vane conventions)
- `dana_figE_windpower_vs_dist.png`- Wind power density vs distance
- `dana_figF_windrose.png`         - Wind rose (count of samples per direction)

It also writes `analyze_dana_console.txt` with the headline numbers
(mean wind speed, wind power density, energy ratios, distance-binned means).

## How to inspect a single log in detail

`logreader_project.m` is the right tool if you want one figure per sensor
(one for anemometer, one for vane, one for BPW34, etc.) instead of the
combined plots.

1. Drop your `LOG###.BIN` and `INF###.TXT` files onto the Desktop.
2. Open `logreader_project.m` and set `filenum` near the top to your log
   number (e.g. `'009'`).
3. Run.

## How to convert raw SD-card logs into CSVs

Only needed if you have new `.BIN` / `.INF` files from the deployment SD
card and want to merge them into the analysis pipeline.

1. Drop the BIN/INF files onto the Desktop (or set `dataDir` in the
   script).
2. Open `logreader_danapoint.m` and set `filenum` and `deployment` (set
   `'dana'` for harbor day).
3. Run. It writes a CSV into `final-writeup/data-deployment/dana-raw/`.

## Notes

- The distance-from-shore axis is capped at 70 m (~200 ft) on the
  visualizations. Beyond that, the boat was moving westward along the cove
  rather than further from any shore line, so the "distance from shore"
  abstraction breaks. The full path data is still loaded, just not plotted
  past 70 m.
- The wind rose uses the calibrated INTO-wind vane convention. If your
  petals point the wrong way after re-running, switch the source column to
  `wind_dir_away_deg` in the figure F block.
- The `dataDir` in `analyze_dana.m` resolves automatically to the
  `dana-raw/` folder next to the script, so the bundle is portable.
