% logreader_project.m

% 2. "How do I know what columns exist?"
%    -> Open the INF*.TXT file from your SD card in a text editor. It's a
%       CSV listing all variable names and their types. The names match
%       the first argument of each DataSource constructor (e.g.,
%       "als_raw,lux" in SensorVEML6030.cpp). The logreader's eval()
%       call creates MATLAB variables with these exact names.
%
% 3. "How do I convert ADC values to voltage?"
%    -> Teensy 4.0 default is 10-bit, but SensorWeatherVane::init() calls
%       analogReadResolution(12), which sets it GLOBALLY for all
%       analogRead() calls (including ADCSampler). So the ADC columns
%       are 12-bit (0-4095), and voltage = raw * 3.3 / 4095.
%       If you ever run a sketch without the weather vane, revert this
%       back to 1023. Arduino analogRead() reference covers the details.
%
% 4. "How do I identify station-stop periods?"
%    -> During STATION state, the .ino sets motorA=0 and motorB=0.
%       So (motorA == 0 & motorB == 0) marks stopped periods. Use
%       diff() to find transitions (start/end of each stopped period).

%
% Expected log columns:
%   IMU:       rollIMU, pitchIMU, headingIMU, accelX, accelY, accelZ,
%              magX, magY, magZ
%   GPS:       lat, lon, nsats
%   XY State:  x, y
%   Motors:    motorA, motorB, motorC
%   ADC:       Current_Sense, A00, A01, A02, A03, A10, A11, A12, A13
%              (A00 = pin 14, noise -- used for anemometer digital interrupt)
%              (A01 = pin 15, weather vane raw ADC -- same data as vane_voltage)
%              (A02 = pin 16, BPW34 TIA output)
%   Errors:    ErrorFlagA, ErrorFlagB, ErrorFlagC
%   Button:    Button
%   VEML6030:  als_raw, lux
%   Anemometer: anemo_freq, wind_speed
%   WeatherVane: vane_angle, vane_voltage

clear;

filenum = '038';  % file number for the data you want to read
dataDir = fullfile(getenv('HOME'), 'Desktop');  % where LOG/INF live
infofile = fullfile(dataDir, strcat('INF', filenum, '.TXT'));
datafile = fullfile(dataDir, strcat('LOG', filenum, '.BIN'));

if ~isfile(infofile)
    error('INF file not found: %s\nDrop INF%s.TXT onto your Desktop, or change dataDir above.', ...
          infofile, filenum);
end
if ~isfile(datafile)
    error('LOG file not found: %s\nDrop LOG%s.BIN onto your Desktop, or change dataDir above.', ...
          datafile, filenum);
end

%% map from datatype to length in bytes
dataSizes.('float') = 4;
dataSizes.('ulong') = 4;
dataSizes.('int') = 4;
dataSizes.('int32') = 4;
dataSizes.('uint8') = 1;
dataSizes.('uint16') = 2;
dataSizes.('char') = 1;
dataSizes.('bool') = 1;

%% read from info file to get log file structure
fileID = fopen(infofile);
items = textscan(fileID, '%s', 'Delimiter', ',', 'EndOfLine', '\r\n');
fclose(fileID);
[ncols, ~] = size(items{1});
ncols = ncols / 2;
varNames = items{1}(1:ncols)';
varTypes = items{1}(ncols+1:end)';
varLengths = zeros(size(varTypes));
colLength = 256;
for i = 1:numel(varTypes)
    varLengths(i) = dataSizes.(varTypes{i});
end
R = cell(1, numel(varNames));

%% read column-by-column from datafile
fid = fopen(datafile, 'rb');
for i = 1:numel(varTypes)
    fseek(fid, sum(varLengths(1:i-1)), 'bof');
    R{i} = fread(fid, Inf, ['*' varTypes{i}], colLength - varLengths(i));
    eval(strcat(varNames{i}, '=', 'R{', num2str(i), '};'));
end
fclose(fid);

%% Build time axis
dt = 0.099;  % loop period in seconds (LOOP_PERIOD = 99 ms)
N = length(R{1});
t = ((0:N-1) * dt)';
t_min = t / 60;  % time in minutes for readability

%% Report what columns were actually loaded
fprintf('Loaded %d samples from %s\n', N, datafile);
fprintf('Columns in this log: %s\n', strjoin(varNames, ', '));

%% Compute BPW34 voltage (prefer real A02 ADC, fall back to synthetic)
% Real sensor path: ADCSampler writes A02 as uint16 counts -> convert to V.
% Synthetic path: patch_synthetic.m writes bpw_voltage directly as a float
% voltage, so no conversion needed.
if exist('A02', 'var')
    bpw34_voltage = double(A02) * 3.3 / 4095;
elseif exist('bpw_voltage', 'var')
    bpw34_voltage = double(bpw_voltage);      % synthetic, already volts
    fprintf('Using synthetic bpw_voltage (from patch_synthetic.m)\n');
else
    bpw34_voltage = NaN(N, 1);
end

if exist('A01', 'var')
    vane_voltage_raw = double(A01) * 3.3 / 4095;
else
    vane_voltage_raw = NaN(N, 1);
end

%% Alias synthetic lux column onto 'lux' if the real VEML wasn't logged
if ~exist('lux', 'var') && exist('veml_lux', 'var')
    lux = double(veml_lux);
    fprintf('Using synthetic veml_lux (from patch_synthetic.m)\n');
end

%% Fill any still-missing columns with NaN so plotting doesn't crash
if ~exist('lux', 'var'),         lux         = NaN(N, 1); end
if ~exist('als_raw', 'var'),     als_raw     = NaN(N, 1); end
if ~exist('wind_speed', 'var'),  wind_speed  = NaN(N, 1); end
if ~exist('vane_angle', 'var'),  vane_angle  = NaN(N, 1); end
if ~exist('headingIMU', 'var'),  headingIMU  = NaN(N, 1); end
if ~exist('motorA', 'var'),      motorA      = zeros(N, 1); end
if ~exist('motorB', 'var'),      motorB      = zeros(N, 1); end

%% Identify station periods (motors off = station stop)
% motorA and motorB are both 0 when stopped
is_stopped = (motorA == 0) & (motorB == 0);

%% Decide which plots to build based on what's in this log
haveWind = any(~isnan(wind_speed));
haveLux  = any(~isnan(lux));
haveBpw  = any(~isnan(bpw34_voltage));
haveVane = any(~isnan(vane_angle));
haveHead = any(~isnan(headingIMU));

%% ---- One dedicated figure per sensor ----
% Each signal gets its own full-size figure with proper title, units, and
% x-axis. Station periods are shaded green where motors were off >= 30 s.
figNum = 0;

if haveWind
    figNum = figNum + 1; figure(figNum); clf;
    plot(t_min, wind_speed, 'b', 'LineWidth', 1.0); grid on;
    xlabel('Time (min)'); ylabel('Wind speed (m/s)');
    title(sprintf('Anemometer wind speed (LOG%s)', filenum));
    shadeStations(t_min, is_stopped, dt, 30);
end

if haveVane
    figNum = figNum + 1; figure(figNum); clf;
    plot(t_min, vane_angle, 'c', 'LineWidth', 1.0); grid on;
    xlabel('Time (min)'); ylabel('Vane angle (deg, robot frame)');
    ylim([0 360]);
    title(sprintf('Weather vane angle (LOG%s)', filenum));
    shadeStations(t_min, is_stopped, dt, 30);
end

if haveBpw
    figNum = figNum + 1; figure(figNum); clf;
    plot(t_min, bpw34_voltage, 'm', 'LineWidth', 1.0); grid on;
    xlabel('Time (min)'); ylabel('BPW34 voltage (V)');
    title(sprintf('BPW34 photodiode + TIA (LOG%s, A02 / pin 16)', filenum));
    shadeStations(t_min, is_stopped, dt, 30);
end

if haveLux
    figNum = figNum + 1; figure(figNum); clf;
    plot(t_min, lux, 'r', 'LineWidth', 1.0); grid on;
    xlabel('Time (min)'); ylabel('Illuminance (lux)');
    title(sprintf('VEML6030 ambient light (LOG%s)', filenum));
    shadeStations(t_min, is_stopped, dt, 30);
end

if haveHead
    figNum = figNum + 1; figure(figNum); clf;
    plot(t_min, headingIMU, 'k', 'LineWidth', 1.0); grid on;
    xlabel('Time (min)'); ylabel('Heading (deg, true)');
    ylim([0 360]);
    title(sprintf('IMU heading (LOG%s, LSM303AGR)', filenum));
    shadeStations(t_min, is_stopped, dt, 30);
end

%% GPS track (if satellites were acquired)
if exist('lat', 'var') && exist('lon', 'var')
    has_gps = (nsats >= 5);
    if any(has_gps)
        figNum = figNum + 1; figure(figNum); clf;
        plot(double(lon(has_gps)), double(lat(has_gps)), 'b.-');
        xlabel('Longitude'); ylabel('Latitude');
        title(sprintf('GPS track (LOG%s)', filenum));
        grid on; axis equal;
        hold on;
        stopped_gps = has_gps & is_stopped;
        if any(stopped_gps)
            plot(double(lon(stopped_gps)), double(lat(stopped_gps)), ...
                 'ro', 'MarkerSize', 4);
            legend('Track', 'Station stops', 'Location', 'best');
        else
            legend('Track', 'Location', 'best');
        end
        hold off;
    end
end

%% Station-averaged data summary (only for motor-off runs >= 30 s)
STATION_MIN_S = 30;
min_samples   = round(STATION_MIN_S / dt);

d = diff([0; is_stopped(:); 0]);
run_starts = find(d ==  1);
run_ends   = find(d == -1) - 1;
keep       = (run_ends - run_starts + 1) >= min_samples;
stop_starts = run_starts(keep);
stop_ends   = run_ends(keep);
n_stations  = numel(stop_starts);

if n_stations > 0
    fprintf('\n=== Station Averages (runs >= %d s) ===\n', STATION_MIN_S);
    fprintf('%-8s %-10s %-10s %-10s %-12s %-10s\n', ...
            'Station', 'Wind m/s', 'Lux', 'BPW V', 'Vane deg', 'Head deg');
    fprintf('%s\n', repmat('-', 1, 70));
    for s = 1:n_stations
        idx = stop_starts(s):stop_ends(s);
        fprintf('%-8d %-10.2f %-10.1f %-10.3f %-12.1f %-10.1f\n', ...
                s, ...
                mean(wind_speed(idx), 'omitnan'), ...
                mean(lux(idx), 'omitnan'), ...
                mean(bpw34_voltage(idx), 'omitnan'), ...
                mean(double(vane_angle(idx)), 'omitnan'), ...
                mean(headingIMU(idx), 'omitnan'));
    end
else
    fprintf('\nNo station stops detected (no motor-off runs >= %d s)\n', STATION_MIN_S);
end

fprintf('\nTotal samples: %d (%.1f min @ 10 Hz)\n', N, t_min(end));


%% ---- Helpers ----
function shadeStations(t_min, is_stopped, dt, minSeconds)
    % Shade motor-off runs lasting >= minSeconds on the current axes.
    if ~any(is_stopped), return; end
    min_samples = round(minSeconds / dt);
    d = diff([0; is_stopped(:); 0]);
    starts = find(d ==  1);
    ends   = find(d == -1) - 1;
    hold on;
    yl = ylim;
    for k = 1:numel(starts)
        if (ends(k) - starts(k) + 1) >= min_samples
            xs = [t_min(starts(k)) t_min(ends(k))];
            patch([xs(1) xs(2) xs(2) xs(1)], ...
                  [yl(1) yl(1) yl(2) yl(2)], ...
                  'g', 'FaceAlpha', 0.1, 'EdgeColor', 'none');
        end
    end
    hold off;
end

fprintf('\nTotal samples: %d (%.1f minutes)\n', N, t_min(end));
