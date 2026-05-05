% logreader_danapoint.m
%
% Reads DanaPointClosed or DanaPointOpen binary logs (LOG*.BIN + INF*.TXT).
%
% Usage:
%   1. Put LOG<nnn>.BIN and INF<nnn>.TXT from the SD card in this folder
%      (or set dataDir below to point at wherever they live).
%   2. Set filenum to the three-digit log number.
%   3. Run.
%
% The script auto-detects which sketch produced the log:
%   - DanaPointClosed: has Button, x, y columns (state machine + button start)
%   - DanaPointOpen:   no Button, no x/y (open-loop countdown start)
%
% Column mapping (matches logger.include() order in the .ino files):
%   IMU:         rollIMU, pitchIMU, headingIMU, accelX, accelY, accelZ,
%                magX, magY, magZ
%   GPS:         lat, lon, nsats
%   XY (closed): x, y
%   Motors:      motorA, motorB, motorC        (B is usually 0 for our config)
%   ADC:         Current_Sense, A00, A01, A02, A03, A10, A11, A12, A13
%                (A00 = pin 14 = anemometer digital, garbage analog -- ignore)
%                (A01 = pin 15 = weather vane raw ADC)
%                (A02 = pin 16 = BPW34 TIA output)
%   Errors:      ErrorFlagA, ErrorFlagB, ErrorFlagC
%   Button:      Button (closed only)
%   VEML6030:    als_raw, lux
%   Anemometer:  anemo_freq, wind_speed, anemo_pulses
%   WeatherVane: vane_angle, vane_voltage
%   NavLogger:   tgt_hdg, clean_hdg, hdg_err   (DanaPointClosedShort only)
%
% ADC is 12-bit (0-4095). SensorWeatherVane::init() calls
% analogReadResolution(12), which sets it globally on the Teensy --
% so all ADCSampler columns are 12-bit too. voltage = raw * 3.3 / 4095.

clear;

%% -------- User settings --------
filenum    = '021';                                       % LOG###.BIN / INF###.TXT
deployment = 'phake';                                      % 'dana' or 'phake' -> picks output folder
dataDir    = fullfile(getenv('HOME'), 'Desktop/phake');    % where SD-card LOG/INF live

%% -------- Output folder (auto, based on deployment) --------
WRITEUP_ROOT = '/Users/felixpeng/Downloads/repos/E80-Team-32/final-writeup/data-deployment';
switch lower(deployment)
    case 'dana',  outputDir = fullfile(WRITEUP_ROOT, 'dana-raw');
    case 'phake', outputDir = fullfile(WRITEUP_ROOT, 'phake-raw');
    otherwise,    error('deployment must be ''dana'' or ''phake'' (got: %s)', deployment);
end
if ~exist(outputDir, 'dir'), mkdir(outputDir); end

%% -------- Mirror console output to text file --------
diaryPath = fullfile(outputDir, ['LOG' filenum '_console.txt']);
if exist(diaryPath, 'file'), delete(diaryPath); end
diary(diaryPath); diary on;
cleanupDiary = onCleanup(@() diary('off'));   %#ok<NASGU>
fprintf('=== logreader_danapoint.m: %s LOG%s ===\n', deployment, filenum);
fprintf('Run timestamp: %s\n\n', datestr(now, 'yyyy-mm-dd HH:MM:SS'));

%% -------- Derived file paths --------
infofile = fullfile(dataDir, strcat('INF', filenum, '.TXT'));
datafile = fullfile(dataDir, strcat('LOG', filenum, '.BIN'));

%% -------- Datatype -> bytes map (used to seek through the 256-byte block) --------
dataSizes.('float')  = 4;
dataSizes.('ulong')  = 4;
dataSizes.('int')    = 4;
dataSizes.('int32')  = 4;
dataSizes.('uint8')  = 1;
dataSizes.('uint16') = 2;
dataSizes.('char')   = 1;
dataSizes.('bool')   = 1;

%% -------- Parse INF file for column schema --------
fileID = fopen(infofile);
items = textscan(fileID, '%s', 'Delimiter', ',', 'EndOfLine', '\r\n');
fclose(fileID);

[ncols, ~] = size(items{1});
ncols = ncols / 2;
varNames   = items{1}(1:ncols)';
varTypes   = items{1}(ncols+1:end)';
varLengths = zeros(size(varTypes));
colLength  = 256;
for i = 1:numel(varTypes)
    varLengths(i) = dataSizes.(varTypes{i});
end

%% -------- Read column-major binary log --------
R = cell(1, numel(varNames));
fid = fopen(datafile, 'rb');
for i = 1:numel(varTypes)
    fseek(fid, sum(varLengths(1:i-1)), 'bof');
    R{i} = fread(fid, Inf, ['*' varTypes{i}], colLength - varLengths(i));
    eval(strcat(varNames{i}, '=', 'R{', num2str(i), '};'));
end
fclose(fid);

%% -------- Build time axis --------
dt    = 0.099;                 % LOOP_PERIOD = 99 ms
N     = length(R{1});
t     = ((0:N-1) * dt)';
t_min = t / 60;

fprintf('Loaded %d samples (%.1f min) from %s\n', N, t_min(end), datafile);

%% -------- Detect sketch variant --------
isClosed = exist('Button', 'var') && exist('x', 'var') && exist('y', 'var');
if isClosed
    sketchLabel = 'DanaPointClosed';
else
    sketchLabel = 'DanaPointOpen';
end
fprintf('Sketch detected: %s\n', sketchLabel);

%% -------- Defensive defaults for columns absent in older sketches --------
% Older logs (e.g. early Phake Lake) may not include every ADC channel or
% every sensor stream. Fill missing names with NaN so plotting / averaging
% downstream don't crash. CSV export still skips these via `ismember` so
% the saved CSV doesn't get junk NaN columns.
if ~exist('A01', 'var'),         A01         = nan(N, 1); end
if ~exist('A02', 'var'),         A02         = nan(N, 1); end
if ~exist('lux', 'var'),         lux         = nan(N, 1); end
if ~exist('als_raw', 'var'),     als_raw     = nan(N, 1); end
if ~exist('wind_speed', 'var'),  wind_speed  = nan(N, 1); end
if ~exist('anemo_freq', 'var'),  anemo_freq  = nan(N, 1); end
if ~exist('vane_angle', 'var'),  vane_angle  = nan(N, 1); end
if ~exist('vane_voltage', 'var'),vane_voltage= nan(N, 1); end
if ~exist('headingIMU', 'var'),  headingIMU  = nan(N, 1); end
if ~exist('motorA', 'var'),      motorA      = zeros(N, 1); end
if ~exist('motorB', 'var'),      motorB      = zeros(N, 1); end
if ~exist('motorC', 'var'),      motorC      = zeros(N, 1); end

%% -------- ADC -> voltage conversions (12-bit, 3.3V ref) --------
ADC_MAX = 4095;
VREF    = 3.3;
bpw34_voltage_raw = double(A02) * VREF / ADC_MAX;   % pin 16 = BPW34 TIA (NaN if absent)
vane_voltage_raw  = double(A01) * VREF / ADC_MAX;   % pin 15 = MLX90316 raw (NaN if absent)

%% -------- Station / pause detection --------
% Motor B is a small trim, so check A and C instead. Distinguish stations
% (long off runs) from the closed sketches' 500 ms mag-sample pauses.
% Threshold: 12 s catches ClosedShort's 15 s stations while still filtering
% out Open's 10 s sensor-collection pauses if the log happens to be an
% Open log (Open has no real "stations", just brief pauses).
motors_off = (motorA == 0) & (motorC == 0);

STATION_MIN_S = 12;
min_samples   = round(STATION_MIN_S / dt);

is_station = false(size(motors_off));
if any(motors_off)
    d = diff([0; motors_off; 0]);
    run_starts = find(d == 1);
    run_ends   = find(d == -1) - 1;
    for k = 1:length(run_starts)
        if (run_ends(k) - run_starts(k) + 1) >= min_samples
            is_station(run_starts(k):run_ends(k)) = true;
        end
    end
end

%% -------- Time-series plots --------
figure(1); clf;
sgtitle([sketchLabel ' - Sensor Data vs Time'], 'Interpreter', 'none');

% 1. Wind speed
subplot(6, 1, 1);
plot(t_min, wind_speed, 'b'); grid on;
ylabel('Wind (m/s)'); title('Anemometer');
shadeStations(t_min, is_station);

% 2. VEML lux
subplot(6, 1, 2);
plot(t_min, lux, 'r'); grid on;
ylabel('Lux'); title('VEML6030 Ambient Light');

% 3. BPW34 voltage (from A02)
subplot(6, 1, 3);
plot(t_min, bpw34_voltage_raw, 'm'); grid on;
ylabel('Volts'); title('BPW34 Photodiode (A02)');

% 4. Weather vane angle
subplot(6, 1, 4);
plot(t_min, vane_angle, 'c'); grid on;
ylabel('deg'); title('Weather Vane Direction');

% 5. IMU heading
subplot(6, 1, 5);
plot(t_min, headingIMU, 'k'); grid on;
ylabel('deg'); title('IMU Heading');

% 6. Motor commands (including C since we're using A+C)
subplot(6, 1, 6);
plot(t_min, motorA, 'b', 'DisplayName', 'Motor A'); hold on;
plot(t_min, motorB, 'g', 'DisplayName', 'Motor B');
plot(t_min, motorC, 'r', 'DisplayName', 'Motor C');
hold off; grid on; legend('Location', 'best');
ylabel('Cmd'); xlabel('Time (min)'); title('Motor Drive');

%% -------- GPS track --------
if exist('lat', 'var') && exist('lon', 'var') && any(nsats >= 5)
    has_gps = (nsats >= 5);
    figure(2); clf;
    plot(double(lon(has_gps)), double(lat(has_gps)), 'b.-');
    xlabel('Longitude'); ylabel('Latitude');
    title([sketchLabel ' - GPS Track'], 'Interpreter', 'none');
    grid on; axis equal;

    if any(is_station & has_gps)
        hold on;
        stopped_gps = is_station & has_gps;
        plot(double(lon(stopped_gps)), double(lat(stopped_gps)), ...
             'ro', 'MarkerSize', 4, 'DisplayName', 'Stations');
        legend('Track', 'Stations', 'Location', 'best');
        hold off;
    end
else
    fprintf('No GPS lock (nsats < 5 for all samples) -- skipping map plot\n');
end

%% -------- Station-averaged summary (closed sketch only) --------
if isClosed && any(is_station)
    stop_starts = find(diff([0; is_station]) ==  1);
    stop_ends   = find(diff([is_station; 0]) == -1);
    n_stations  = min(length(stop_starts), length(stop_ends));

    fprintf('\n=== Station Averages (%s) ===\n', sketchLabel);
    fprintf('%-4s %-10s %-8s %-10s %-12s %-10s %-10s %-10s\n', ...
            'Stn', 'Wind m/s', 'Lux', 'BPW V', 'Vane deg', 'Head deg', ...
            'Lat', 'Lon');
    fprintf('%s\n', repmat('-', 1, 86));
    for s = 1:n_stations
        idx = stop_starts(s):stop_ends(s);
        fprintf('%-4d %-10.2f %-8.0f %-10.3f %-12.1f %-10.1f %-10.5f %-10.5f\n', ...
                s, ...
                mean(wind_speed(idx)), ...
                mean(lux(idx)), ...
                mean(bpw34_voltage_raw(idx)), ...
                mean(double(vane_angle(idx))), ...
                mean(headingIMU(idx)), ...
                mean(double(lat(idx))), ...
                mean(double(lon(idx))));
    end
elseif ~isClosed
    fprintf('\nOpen-loop sketch -- no stations to summarize\n');
end

fprintf('\nTotal samples: %d (%.1f minutes @ %.0f Hz)\n', N, t_min(end), 1/dt);


%% -------- Magnetometer diagnostic (2D only: surface vehicle stays level) --------
% If the boat is physically rotating but headingIMU is flat, mag is either
% saturated / shielded (tight blob) or off-center (circle whose center is
% far from origin -> atan2 pins to one quadrant). Center and radius estimates
% below tell you which.
if exist('magX', 'var') && exist('magY', 'var')
    mx = double(magX); my = double(magY);

    cx = mean(mx); cy = mean(my);
    r  = sqrt((mx - cx).^2 + (my - cy).^2);
    r_mean   = mean(r);
    r_spread = std(r) / max(r_mean, eps);   % should be small if truly on a circle

    figure(3); clf;
    sgtitle('Magnetometer diagnostic (2D)');

    subplot(1, 2, 1);
    plot(mx, my, 'b.', 'MarkerSize', 4); grid on; axis equal; hold on;
    plot(cx, cy, 'rx', 'MarkerSize', 12, 'LineWidth', 2);
    theta = linspace(0, 2*pi, 200);
    plot(cx + r_mean*cos(theta), cy + r_mean*sin(theta), 'r--');
    hold off;
    xlabel('magX (uT)'); ylabel('magY (uT)');
    title(sprintf('Scatter: center=(%.1f, %.1f), r=%.1f, spread=%.2f', ...
                  cx, cy, r_mean, r_spread));

    subplot(1, 2, 2);
    plot(t_min, mx, 'b', 'DisplayName', 'magX'); hold on;
    plot(t_min, my, 'r', 'DisplayName', 'magY');
    hold off; grid on; legend('Location', 'best');
    xlabel('Time (min)'); ylabel('uT'); title('Mag axes vs time');

    fprintf('\n=== Mag diagnostic ===\n');
    fprintf('  Center:        (%.2f, %.2f) uT   (should be near origin after cal)\n', cx, cy);
    fprintf('  Mean radius:    %.2f uT           (Earth field is ~25-65 uT)\n', r_mean);
    fprintf('  Relative spread: %.3f              (tight circle -> <0.05; saturated blob -> ~0)\n', r_spread);
    if r_mean < 3
        fprintf('  ** Readings nearly constant - mag is saturated or shielded.\n');
    elseif hypot(cx, cy) > 0.5 * r_mean
        fprintf('  ** Center is far from origin - hard-iron offset needs calibration.\n');
    end
end

%% -------- Controller state (DanaPointClosedShort with NavLogger) --------
% Shows what the P-controller was actually seeing. If `hdg_err` stays near
% zero the whole run but the boat circled, the controller was starved of
% real feedback (mag saturated / lastCleanHeading never changed).
if exist('tgt_hdg', 'var') && exist('clean_hdg', 'var') && exist('hdg_err', 'var')
    figure(4); clf;
    sgtitle('Closed-loop controller state');

    subplot(2, 1, 1);
    plot(t_min, headingIMU, 'Color', [0.6 0.6 0.6], 'DisplayName', 'headingIMU (raw, 10 Hz)');
    hold on;
    plot(t_min, clean_hdg,  'b', 'LineWidth', 1.4, 'DisplayName', 'clean\_hdg (pause-averaged)');
    plot(t_min, tgt_hdg,    'g--', 'LineWidth', 1.2, 'DisplayName', 'tgt\_hdg');
    hold off; grid on; legend('Location', 'best');
    ylabel('deg'); title('Heading: raw vs clean vs target');
    shadeStations(t_min, is_station);

    subplot(2, 1, 2);
    plot(t_min, hdg_err, 'r'); grid on; hold on;
    yline(0, 'k:');
    hold off;
    ylabel('deg'); xlabel('Time (min)');
    title('Heading error (target - clean), fed into KP_HEADING');
    shadeStations(t_min, is_station);

    fprintf('\n=== Controller diagnostic ===\n');
    fprintf('  clean_hdg range: [%.1f, %.1f] deg   (should sweep if boat rotates)\n', ...
            min(clean_hdg), max(clean_hdg));
    fprintf('  mean |hdg_err|:   %.2f deg         (if ~0 while boat circles -> mag feedback is dead)\n', ...
            mean(abs(hdg_err)));
end

%% -------- Anemometer pulse count (new column) --------
if exist('anemo_pulses', 'var')
    figure(5); clf;
    plot(t_min, double(anemo_pulses), 'b'); grid on;
    xlabel('Time (min)'); ylabel('pulses');
    title('Anemometer live pulse count (resets each 10 s window)');
    shadeStations(t_min, is_station);
end


%% -------- Export CSV to outputDir --------
% Grab every column the sketch logged, plus convenience columns derived
% from ADC channels and from sensor fusion.

csvPath = fullfile(outputDir, ['LOG' filenum '.csv']);

tbl = table();
tbl.time_s = t(:);

% Direct passthroughs (everything the INF declared)
for i = 1:numel(varNames)
    v = eval(varNames{i});
    tbl.(varNames{i}) = double(v(:));
end

% Derived voltages (only if the underlying ADC channel was actually logged --
% checked against the INF schema so NaN-padded defaults don't end up in the CSV)
if ismember('A02', varNames)
    tbl.bpw34_voltage = bpw34_voltage_raw(:);
end
if ismember('A01', varNames)
    tbl.vane_voltage_from_adc = vane_voltage_raw(:);
end

% Derived: true wind direction in earth frame = (vane_angle + headingIMU) mod 360
% vane_angle is wind direction relative to robot; headingIMU is robot bearing.
if ismember('vane_angle', varNames) && ismember('headingIMU', varNames)
    tbl.wind_dir_true_deg = mod(double(vane_angle) + double(headingIMU), 360);
end

% Station index (0 during drive, 1 for station 1 period, 2 for station 2, ...)
station_idx = zeros(N, 1);
if any(is_station)
    d = diff([0; is_station(:); 0]);
    starts = find(d ==  1);
    ends   = find(d == -1) - 1;
    for k = 1:numel(starts)
        station_idx(starts(k):ends(k)) = k;
    end
end
tbl.station_number = station_idx;

writetable(tbl, csvPath);
fprintf('CSV exported: %s\n', csvPath);


%% -------- Save figures to outputDir --------
saveFigIfOpen(1, fullfile(outputDir, ['LOG' filenum '_fig1_timeseries.png']));
saveFigIfOpen(2, fullfile(outputDir, ['LOG' filenum '_fig2_gps.png']));
saveFigIfOpen(3, fullfile(outputDir, ['LOG' filenum '_fig3_mag.png']));
saveFigIfOpen(4, fullfile(outputDir, ['LOG' filenum '_fig4_controller.png']));
saveFigIfOpen(5, fullfile(outputDir, ['LOG' filenum '_fig5_anemo.png']));

fprintf('\nAll outputs in: %s\n', outputDir);
fprintf('  - LOG%s_console.txt   (this console output)\n', filenum);
fprintf('  - LOG%s.csv           (full data, every logged + derived column)\n', filenum);
fprintf('  - LOG%s_fig*.png      (each MATLAB figure that was generated)\n', filenum);

diary off;


%% -------- Helpers --------
function shadeStations(t_min, is_station)
    % Shade station windows on the current axes.
    if ~any(is_station), return; end
    yl = ylim;
    d  = diff([0; is_station(:); 0]);
    starts = find(d ==  1);
    ends   = find(d == -1) - 1;
    hold on;
    for k = 1:length(starts)
        xs = [t_min(starts(k)) t_min(ends(k))];
        patch([xs(1) xs(2) xs(2) xs(1)], [yl(1) yl(1) yl(2) yl(2)], ...
              'g', 'FaceAlpha', 0.1, 'EdgeColor', 'none');
    end
    hold off;
end

function saveFigIfOpen(figNum, outPath)
    % Save figure(figNum) to outPath as PNG, but only if it was actually
    % created during this run. Skips silently otherwise.
    h = findobj('Type', 'figure', 'Number', figNum);
    if isempty(h), return; end
    try
        exportgraphics(h, outPath, 'Resolution', 200);
    catch
        % Fallback for older MATLAB without exportgraphics
        print(h, outPath, '-dpng', '-r200');
    end
    fprintf('Saved figure %d -> %s\n', figNum, outPath);
end
