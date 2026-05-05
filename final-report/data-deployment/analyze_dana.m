% analyze_dana.m
%
% Cross-run analysis for the Dana Point deployment.
% Reads the per-log CSVs produced by logreader_danapoint.m, concatenates
% them, computes distance-from-shore for every GPS sample, and produces the
% headline figures we need for the Results section of the report.
%
% Prereqs:
%   - LOG007.csv, LOG008.csv, LOG009.csv (or whichever Dana logs are present)
%     in the same folder as this script.
%
% Outputs (alongside this script):
%   - dana_combined.csv                 -- all logs concatenated, with derived
%                                          distance_from_shore_m column added
%   - analyze_dana_console.txt          -- console output (diary capture)
%   - dana_figA_gps_track.png           -- combined GPS track, shore ref marked
%   - dana_figB_bpw_vs_dist.png         -- BPW34 voltage vs distance from shore
%   - dana_figC_wind_vs_dist.png        -- wind speed vs distance from shore
%   - dana_figD_winddir_vs_dist.png     -- true wind direction vs distance
%
% Shore reference: easternmost GPS point in the combined track. Felix
% confirmed this is right at the beach (a few meters from where sand meets
% waves) -- so all other points are westward = further offshore.
%
% Distance metric: 2D Euclidean distance via equirectangular approximation
% (small area, sub-500 m, error well under a meter at this latitude).

clear; close all;

%% -------- User settings --------
dataDir = '/Users/felixpeng/Downloads/repos/E80-Team-32/final-writeup/data-deployment/dana-raw';
logs    = {'LOG007', 'LOG008', 'LOG009'};

% Below this wind speed the vane direction is unreliable (cup anemometer
% barely turns; vane sits wherever it was last pushed). Used to filter the
% wind direction plot and the wind rose so we don't visualize garbage.
WIND_DIR_MIN_MS = 1.5;

% Distance-from-shore axis cap. Beyond this, the boat was moving westward
% along the cove rather than actually further from any shore line, so the
% "distance from shore" abstraction breaks. 70 m ~ 200 ft was the meaningful
% offshore reach for the Dana Cove deployment.
DIST_PLOT_MAX_M = 70;

% Vane mechanical-zero calibration. When the vane was physically pointed
% along the boat's bow direction during pre-deployment bench check, the
% Teensy ADC measured this voltage. Using the same linear map the firmware
% uses (in SensorWeatherVane.cpp): angle = 360 * (V - VMIN) / (VMAX - VMIN),
% with VMIN = 0.094 V and VMAX = 3.000 V, this voltage corresponds to the
% raw vane_angle that means "wind from straight ahead." Subtract this from
% every vane sample to put 0 deg = wind from the bow in the boat-relative
% frame.
VANE_FORWARD_VOLTAGE = 0.52;
VANE_VMIN = 0.094;
VANE_VMAX = 3.000;
VANE_ZERO_DEG = 360 * (VANE_FORWARD_VOLTAGE - VANE_VMIN) / (VANE_VMAX - VANE_VMIN);

%% -------- Console capture --------
diaryPath = fullfile(dataDir, 'analyze_dana_console.txt');
if exist(diaryPath, 'file'), delete(diaryPath); end
diary(diaryPath); diary on;
cleanupDiary = onCleanup(@() diary('off'));   %#ok<NASGU>
fprintf('=== analyze_dana.m run %s ===\n\n', datestr(now, 'yyyy-mm-dd HH:MM:SS'));

%% -------- Load + concatenate per-log CSVs --------
T = table();
for k = 1:numel(logs)
    csvPath = fullfile(dataDir, [logs{k} '.csv']);
    if ~isfile(csvPath)
        warning('Missing: %s -- skipping', csvPath);
        continue;
    end
    Tk = readtable(csvPath);
    Tk.log_id = repmat(string(logs{k}), height(Tk), 1);
    fprintf('Loaded %s: %d rows, %d columns\n', logs{k}, height(Tk), width(Tk));

    % outerjoin-style vertcat: align on column names, fill missing with NaN
    if isempty(T)
        T = Tk;
    else
        % synchronise schema: add any missing columns to either side
        missingInT  = setdiff(Tk.Properties.VariableNames, T.Properties.VariableNames);
        missingInTk = setdiff(T.Properties.VariableNames, Tk.Properties.VariableNames);
        for c = missingInT,  T.(c{1})  = nan(height(T), 1);  end
        for c = missingInTk, Tk.(c{1}) = nan(height(Tk), 1); end
        Tk = Tk(:, T.Properties.VariableNames);
        T = [T; Tk];   %#ok<AGROW>
    end
end
fprintf('\nTotal rows: %d across %d logs\n\n', height(T), numel(logs));

%% -------- Filter for valid GPS samples --------
hasFix = (T.nsats >= 5) & (T.lat ~= 0) & (T.lon ~= 0);
fprintf('Rows with GPS lock (nsats >=5, non-zero lat/lon): %d / %d (%.1f%%)\n', ...
        sum(hasFix), height(T), 100*sum(hasFix)/height(T));

%% -------- Shore reference = easternmost GPS point --------
% "Easternmost" = max longitude (least negative). Felix confirmed this point
% is right at the beach for the Dana Point deployment.
[shoreLon, idxShore] = max(T.lon(hasFix));
fixIdx   = find(hasFix);
shoreLat = T.lat(fixIdx(idxShore));
fprintf('Shore reference (easternmost GPS sample): lat=%.6f, lon=%.6f\n', shoreLat, shoreLon);

%% -------- Distance from shore (equirectangular, meters) --------
R_earth  = 6371000;   % m
mean_lat = mean(T.lat(hasFix));   % use mean lat of track for the cos factor
dx = (T.lon - shoreLon) .* (pi/180) .* R_earth .* cos(mean_lat * pi/180);
dy = (T.lat - shoreLat) .* (pi/180) .* R_earth;
T.distance_from_shore_m = sqrt(dx.^2 + dy.^2);

% Mark non-fix rows as NaN so we don't plot garbage
T.distance_from_shore_m(~hasFix) = NaN;

fprintf('Distance from shore: min=%.1f m, max=%.1f m, mean=%.1f m\n\n', ...
        min(T.distance_from_shore_m, [], 'omitnan'), ...
        max(T.distance_from_shore_m, [], 'omitnan'), ...
        mean(T.distance_from_shore_m, 'omitnan'));

%% -------- Wind power density (P/A = 0.5 * rho * v^3, W/m^2) --------
% Sea-level dry-air density approx 1.225 kg/m^3. Air-temp / RH correction was
% scrapped (thermistor unused), so this is a constant-density assumption.
RHO_AIR = 1.225;   % kg/m^3
if ismember('wind_speed', T.Properties.VariableNames)
    T.wind_power_density_Wm2 = 0.5 * RHO_AIR * (T.wind_speed .^ 3);
end

%% -------- Wind direction in earth frame: calibrated --------
% Vane reports raw angle in robot frame [0, 360). Subtract VANE_ZERO_DEG so
% that 0 deg == wind from the bow. Then add headingIMU to convert into earth
% frame. Two conventions tested (does the vane point INTO the wind or AWAY
% from it -- typical weather vanes point INTO so wind comes FROM the
% direction the vane points). Plot both; pick whichever lines up with
% WeatherSpark's 215 deg.
if ismember('vane_angle', T.Properties.VariableNames) && ...
   ismember('headingIMU', T.Properties.VariableNames)
    vane_corr = mod(double(T.vane_angle) - VANE_ZERO_DEG, 360);   % robot frame, 0 = bow

    % Convention A: vane points INTO wind  -> wind FROM = vane_corr + heading
    T.wind_dir_into_deg = mod(vane_corr + double(T.headingIMU), 360);
    % Convention B: vane points AWAY from wind -> wind FROM = vane_corr + heading + 180
    T.wind_dir_away_deg = mod(vane_corr + double(T.headingIMU) + 180, 360);

    % Keep the legacy uncalibrated columns too, for comparison
    T.wind_dir_add_deg = mod(double(T.vane_angle) + double(T.headingIMU), 360);
    T.wind_dir_sub_deg = mod(double(T.headingIMU) - double(T.vane_angle), 360);

    cmean = @(d) mod(atan2d(mean(sind(d), 'omitnan'), mean(cosd(d), 'omitnan')), 360);
    fprintf('=== Wind direction sanity check vs WeatherSpark (~215 deg) ===\n');
    fprintf('  Vane forward-pointing voltage: %.3f V -> raw vane angle = %.1f deg\n', ...
            VANE_FORWARD_VOLTAGE, VANE_ZERO_DEG);
    fprintf('  Calibrated, vane INTO wind  (vane_corr + heading):       %6.1f deg\n', ...
            cmean(T.wind_dir_into_deg(hasFix)));
    fprintf('  Calibrated, vane AWAY wind  (vane_corr + heading + 180): %6.1f deg\n', ...
            cmean(T.wind_dir_away_deg(hasFix)));
    fprintf('  Uncalibrated (vane + heading):                           %6.1f deg\n', ...
            cmean(T.wind_dir_add_deg(hasFix)));
    fprintf('  Uncalibrated (heading - vane):                           %6.1f deg\n', ...
            cmean(T.wind_dir_sub_deg(hasFix)));
    fprintf('  WeatherSpark expected (regional):                        ~215.0 deg\n\n');
end

%% -------- Save augmented combined CSV --------
combinedPath = fullfile(dataDir, 'dana_combined.csv');
writetable(T, combinedPath);
fprintf('Combined CSV saved: %s\n\n', combinedPath);

%% -------- Plotting helpers --------
colors = lines(numel(logs));   % distinct color per log

%% -------- Figure A: combined GPS track --------
figure(1); clf; hold on; grid on; axis equal;
title('Dana Point deployment - combined GPS track');
xlabel('Longitude'); ylabel('Latitude');
for k = 1:numel(logs)
    sel = (T.log_id == logs{k}) & hasFix;
    if any(sel)
        plot(T.lon(sel), T.lat(sel), '.', 'Color', colors(k,:), ...
             'MarkerSize', 6, 'DisplayName', logs{k});
    end
end
plot(shoreLon, shoreLat, 'rp', 'MarkerSize', 18, 'MarkerFaceColor', 'r', ...
     'DisplayName', 'Shore reference');
legend('Location', 'best');
exportgraphics(gcf, fullfile(dataDir, 'dana_figA_gps_track.png'), 'Resolution', 200);
fprintf('Saved figure A: GPS track\n');

%% -------- Figure B: BPW34 voltage vs distance from shore --------
if ismember('bpw34_voltage', T.Properties.VariableNames)
    figure(2); clf; hold on; grid on;
    title('BPW34 photodiode voltage vs distance from shore');
    xlabel('Distance from shore (m)'); ylabel('BPW34 voltage (V)');
    for k = 1:numel(logs)
        sel = (T.log_id == logs{k}) & hasFix & ~isnan(T.bpw34_voltage);
        if any(sel)
            scatter(T.distance_from_shore_m(sel), T.bpw34_voltage(sel), 6, ...
                    colors(k,:), 'filled', 'MarkerFaceAlpha', 0.3, ...
                    'DisplayName', logs{k});
        end
    end
    xlim([0 DIST_PLOT_MAX_M]);
    legend('Location', 'best');
    exportgraphics(gcf, fullfile(dataDir, 'dana_figB_bpw_vs_dist.png'), 'Resolution', 200);
    fprintf('Saved figure B: BPW vs distance\n');
end

%% -------- Figure C: wind speed vs distance from shore --------
if ismember('wind_speed', T.Properties.VariableNames)
    figure(3); clf; hold on; grid on;
    title('Wind speed vs distance from shore');
    xlabel('Distance from shore (m)'); ylabel('Wind speed (m/s)');
    for k = 1:numel(logs)
        sel = (T.log_id == logs{k}) & hasFix & ~isnan(T.wind_speed);
        if any(sel)
            scatter(T.distance_from_shore_m(sel), T.wind_speed(sel), 6, ...
                    colors(k,:), 'filled', 'MarkerFaceAlpha', 0.3, ...
                    'DisplayName', logs{k});
        end
    end
    xlim([0 DIST_PLOT_MAX_M]);
    legend('Location', 'best');
    exportgraphics(gcf, fullfile(dataDir, 'dana_figC_wind_vs_dist.png'), 'Resolution', 200);
    fprintf('Saved figure C: wind vs distance\n');
end

%% -------- Figure D: wind direction (both conventions, filtered) --------
% Only show samples where wind_speed >= WIND_DIR_MIN_MS. Below that the cup
% anemometer barely turns and the vane just floats wherever -- direction is
% meaningless and only adds noise.
if ismember('wind_dir_into_deg', T.Properties.VariableNames)
    fastEnough = ismember('wind_speed', T.Properties.VariableNames) & ...
                 (T.wind_speed >= WIND_DIR_MIN_MS);
    if isempty(fastEnough), fastEnough = true(height(T), 1); end

    figure(4); clf;
    sgtitle(sprintf(['Wind direction vs distance (filtered: wind \\geq %.1f m/s, ' ...
                     'vane zero = %.1f deg)'], WIND_DIR_MIN_MS, VANE_ZERO_DEG));

    subplot(2,1,1); hold on; grid on; ylim([0 360]);
    title('Calibrated, vane points INTO wind (vane\_corr + heading)');
    xlabel('Distance from shore (m)'); ylabel('Direction (deg)');
    for k = 1:numel(logs)
        sel = (T.log_id == logs{k}) & hasFix & fastEnough & ~isnan(T.wind_dir_into_deg);
        if any(sel)
            scatter(T.distance_from_shore_m(sel), T.wind_dir_into_deg(sel), 8, ...
                    colors(k,:), 'filled', 'MarkerFaceAlpha', 0.4, ...
                    'DisplayName', logs{k});
        end
    end
    xlim([0 DIST_PLOT_MAX_M]);
    yline(190, 'r--', 'WeatherSpark ~190 deg', 'LabelHorizontalAlignment', 'left');
    legend('Location', 'best');

    subplot(2,1,2); hold on; grid on; ylim([0 360]);
    title('Calibrated, vane points AWAY from wind (vane\_corr + heading + 180)');
    xlabel('Distance from shore (m)'); ylabel('Direction (deg)');
    for k = 1:numel(logs)
        sel = (T.log_id == logs{k}) & hasFix & fastEnough & ~isnan(T.wind_dir_away_deg);
        if any(sel)
            scatter(T.distance_from_shore_m(sel), T.wind_dir_away_deg(sel), 8, ...
                    colors(k,:), 'filled', 'MarkerFaceAlpha', 0.4, ...
                    'DisplayName', logs{k});
        end
    end
    xlim([0 DIST_PLOT_MAX_M]);
    yline(190, 'r--', 'WeatherSpark ~190 deg', 'LabelHorizontalAlignment', 'left');
    legend('Location', 'best');

    exportgraphics(gcf, fullfile(dataDir, 'dana_figD_winddir_vs_dist.png'), 'Resolution', 200);
    fprintf('Saved figure D: wind direction (calibrated, both conventions)\n');
end

%% -------- Figure F: wind rose (polar histogram of direction) --------
% Uses the calibrated INTO-wind convention. If your wind rose petals end up
% pointing to the wrong quadrant, switch the column to wind_dir_away_deg.
if ismember('wind_dir_into_deg', T.Properties.VariableNames) && ...
   ismember('wind_speed', T.Properties.VariableNames)

    sel = hasFix & (T.wind_speed >= WIND_DIR_MIN_MS) & ...
          ~isnan(T.wind_dir_into_deg);
    dirDeg = T.wind_dir_into_deg(sel);
    spdMS  = T.wind_speed(sel);

    if ~isempty(dirDeg)
        figure(6); clf;
        % MATLAB's polarhistogram uses CCW from east; convert meteorological
        % convention (CW from north, "wind FROM") -> math convention.
        thetaMath = deg2rad(90 - dirDeg);

        % Single-panel count rose. The speed-weighted variant (sum of m/s
        % per bin) was dropped because its units are not physically
        % meaningful without further normalization (count * mean speed).
        polarhistogram(thetaMath, deg2rad(0:30:360), ...
                       'Normalization', 'count', 'FaceAlpha', 0.6);
        ax = gca; ax.ThetaZeroLocation = 'top'; ax.ThetaDir = 'clockwise';
        ax.ThetaTickLabel = {'N','30','60','E','120','150','S','210','240','W','300','330'};
        title(sprintf(['Wind rose -- Dana Cove deployment\n' ...
                       'Wind FROM direction (n=%d filtered samples, wind \\geq %.1f m/s)'], ...
                      numel(dirDeg), WIND_DIR_MIN_MS));
        exportgraphics(gcf, fullfile(dataDir, 'dana_figF_windrose.png'), 'Resolution', 200);
        fprintf('Saved figure F: wind rose\n');
    end
end

%% -------- Figure E: wind power density vs distance --------
if ismember('wind_power_density_Wm2', T.Properties.VariableNames)
    figure(5); clf; hold on; grid on;
    title('Wind power density (P/A = 0.5\rhov^3) vs distance from shore');
    xlabel('Distance from shore (m)'); ylabel('Wind power density (W/m^2)');
    for k = 1:numel(logs)
        sel = (T.log_id == logs{k}) & hasFix & ~isnan(T.wind_power_density_Wm2);
        if any(sel)
            scatter(T.distance_from_shore_m(sel), T.wind_power_density_Wm2(sel), 6, ...
                    colors(k,:), 'filled', 'MarkerFaceAlpha', 0.3, ...
                    'DisplayName', logs{k});
        end
    end
    xlim([0 DIST_PLOT_MAX_M]);
    legend('Location', 'best');
    exportgraphics(gcf, fullfile(dataDir, 'dana_figE_windpower_vs_dist.png'), 'Resolution', 200);
    fprintf('Saved figure E: wind power density vs distance\n');
end

%% -------- Energy resource summary (the headline finding) --------
% Compares deployment-measured wind power density to coastal reference
% (WeatherSpark) and CIMIS solar irradiance for the deployment window.
fprintf('\n=== Energy resource summary (the headline finding) ===\n');

if ismember('wind_speed', T.Properties.VariableNames)
    v_meas = mean(T.wind_speed(hasFix), 'omitnan');
    pwd_meas = 0.5 * RHO_AIR * v_meas^3;
    fprintf('Measured wind (deployment, harbor):\n');
    fprintf('  mean v = %.2f m/s -> wind power density = %.1f W/m^2\n', v_meas, pwd_meas);
end

% WeatherSpark Dana Point 9-11 PDT averaged ~10-12 mph -> 4.5-5.4 m/s
v_ws_mph = 11;             % WeatherSpark window mean
v_ws = v_ws_mph * 0.44704;
pwd_ws = 0.5 * RHO_AIR * v_ws^3;
fprintf('Coastal reference (WeatherSpark Dana Point, 09-11 PDT):\n');
fprintf('  mean v = %.2f m/s (%.0f mph) -> wind power density = %.1f W/m^2\n', ...
        v_ws, v_ws_mph, pwd_ws);

% CIMIS Station 241, San Clemente, 2026-04-25, hourly Sol Rad column.
% NOTE: column header reads "Sol Rad (Ly/day)" but the hourly values reported
% there are instantaneous solar radiation in W/m^2 -- the "Ly/day" label is a
% legacy header inherited from the daily summary table. The values fit the
% expected morning-irradiance curve only on this interpretation.
%
% Deployment window 09:00-11:00 PDT = 08:00-10:00 PST -> use 0800 + 0900 PST.
solar_Wm2 = [405, 546];   % 0800 PST, 0900 PST
solar_hours_PST = [8, 9];
fprintf('Solar irradiance (CIMIS Station 241, W/m^2):\n');
for h = 1:numel(solar_Wm2)
    fprintf('  %02d:00 PST  (%02d:00 PDT): %5.1f W/m^2\n', ...
            solar_hours_PST(h), solar_hours_PST(h)+1, solar_Wm2(h));
end
fprintf('  mean over deployment window: %.1f W/m^2\n', mean(solar_Wm2));

if ismember('wind_speed', T.Properties.VariableNames)
    fprintf('\nRatios (these are the report headlines):\n');
    fprintf('  Solar / measured wind:  %.1fx\n', mean(solar_Wm2)/pwd_meas);
    fprintf('  Solar / coastal wind:   %.1fx\n', mean(solar_Wm2)/pwd_ws);
    fprintf('  Coastal wind / harbor wind: %.1fx (harbor sheltering effect)\n', pwd_ws/pwd_meas);
end

%% -------- Distance-binned summary --------
edges = 0:20:max(T.distance_from_shore_m, [], 'omitnan') + 20;
binIdx = discretize(T.distance_from_shore_m, edges);

fprintf('\n=== Distance-binned means (20 m bins) ===\n');
fprintf('%-12s %-8s %-12s %-12s %-12s\n', ...
        'Bin (m)', 'N', 'BPW (V)', 'Wind (m/s)', 'Wind dir (deg)');
fprintf('%s\n', repmat('-', 1, 60));
for b = 1:numel(edges)-1
    sel = (binIdx == b) & hasFix;
    if ~any(sel), continue; end
    binLabel = sprintf('%d-%d', edges(b), edges(b+1));
    bpwMean  = NaN;  if ismember('bpw34_voltage',     T.Properties.VariableNames), bpwMean  = mean(T.bpw34_voltage(sel),     'omitnan'); end
    windMean = NaN;  if ismember('wind_speed',        T.Properties.VariableNames), windMean = mean(T.wind_speed(sel),        'omitnan'); end
    dirMean  = NaN;  if ismember('wind_dir_true_deg', T.Properties.VariableNames), dirMean  = mean(T.wind_dir_true_deg(sel), 'omitnan'); end
    fprintf('%-12s %-8d %-12.3f %-12.2f %-12.1f\n', ...
            binLabel, sum(sel), bpwMean, windMean, dirMean);
end

fprintf('\nAll outputs in: %s\n', dataDir);
diary off;


%% -------- Helper: speed-weighted angular bin counts (for wind rose) --------
function counts = accumWind(thetaRad, speed, edgesRad)
    % Sum 'speed' values within each angular bin for a wind rose's
    % speed-weighted petals. thetaRad and speed must be column vectors.
    counts = zeros(1, numel(edgesRad)-1);
    for b = 1:numel(edgesRad)-1
        in = (thetaRad >= edgesRad(b)) & (thetaRad < edgesRad(b+1));
        counts(b) = sum(speed(in), 'omitnan');
    end
    % Wrap any sample exactly at 2*pi into the last bin
    in = (thetaRad >= edgesRad(end));
    counts(end) = counts(end) + sum(speed(in), 'omitnan');
end
