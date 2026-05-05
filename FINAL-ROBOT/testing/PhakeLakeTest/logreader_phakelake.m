% logreader_phakelake.m
% Reads and plots Phake Lake test data from E80 binary logs.
%
% Usage:
%   1. Copy INFxxx.TXT and LOGxxx.BIN from the SD card to this folder
%      (or your desktop — just update the path below)
%   2. Set filenum to match your file number (e.g., '019')
%   3. Run this script
%
% Phake Lake log columns:
%   IMU:         rollIMU, pitchIMU, headingIMU, accelX, accelY, accelZ,
%                magX, magY, magZ
%   GPS:         lat, lon, nsats
%   Motors:      motorA, motorB, motorC
%   Anemometer:  anemo_freq, wind_speed
%   WeatherVane: vane_angle, vane_voltage

clear; close all;

%% ---- CONFIGURATION ----
% Set file number and path to your log files
filenum = '035';
filepath = '~/Desktop/';  % change if files are elsewhere

infofile = fullfile(filepath, ['inf' filenum '.txt']);
datafile = fullfile(filepath, ['log' filenum '.bin']);

%% ---- PARSE INF FILE ----
% Map from datatype string to byte length
dataSizes.('float') = 4;
dataSizes.('ulong') = 4;
dataSizes.('int') = 4;
dataSizes.('int32') = 4;
dataSizes.('uint8') = 1;
dataSizes.('uint16') = 2;
dataSizes.('char') = 1;
dataSizes.('bool') = 1;

fileID = fopen(infofile);
items = textscan(fileID, '%s', 'Delimiter', ',', 'EndOfLine', '\r\n');
fclose(fileID);
[ncols, ~] = size(items{1});
ncols = ncols / 2;
varNames = items{1}(1:ncols)';
varTypes = items{1}(ncols+1:end)';
varLengths = zeros(size(varTypes));
colLength = 256;  % binary block size
for i = 1:numel(varTypes)
    varLengths(i) = dataSizes.(varTypes{i});
end

%% ---- READ BINARY DATA ----
R = cell(1, numel(varNames));
fid = fopen(datafile, 'rb');
for i = 1:numel(varTypes)
    fseek(fid, sum(varLengths(1:i-1)), 'bof');
    R{i} = fread(fid, Inf, ['*' varTypes{i}], colLength - varLengths(i));
    eval(strcat(varNames{i}, '=', 'R{', num2str(i), '};'));
end
fclose(fid);

%% ---- TIME AXIS ----
dt = 0.099;  % LOOP_PERIOD = 99 ms
N = length(R{1});
t = ((0:N-1) * dt)';
t_sec = t;

fprintf('Loaded %d samples (%.1f seconds / %.1f minutes)\n', N, t_sec(end), t_sec(end)/60);

%% ---- DETECT FORWARD / REVERSE / STOPPED PHASES ----
% Forward: motorA > 0, Reverse: motorA < 0, Stopped: motorA == 0
is_forward = (motorA > 0);
is_reverse = (motorA < 0);
is_stopped = (motorA == 0) & (motorB == 0) & (motorC == 0);

% Find phase transitions
fwd_start = find(diff([0; is_forward]) == 1, 1);
fwd_end   = find(diff([is_forward; 0]) == -1, 1);
rev_start = find(diff([0; is_reverse]) == 1, 1);
rev_end   = find(diff([is_reverse; 0]) == -1, 1);

fprintf('\n=== Phase Timing ===\n');
if ~isempty(fwd_start) && ~isempty(fwd_end)
    fprintf('Forward:  %.1f s  to  %.1f s  (%.1f s)\n', ...
        t_sec(fwd_start), t_sec(fwd_end), t_sec(fwd_end)-t_sec(fwd_start));
end
if ~isempty(rev_start) && ~isempty(rev_end)
    fprintf('Reverse:  %.1f s  to  %.1f s  (%.1f s)\n', ...
        t_sec(rev_start), t_sec(rev_end), t_sec(rev_end)-t_sec(rev_start));
end

%% ---- FIGURE 1: MOTOR & HEADING ----
figure(1); clf;
sgtitle('Phake Lake Test - Motors & Navigation');

subplot(3,1,1);
plot(t_sec, motorA, 'b', t_sec, motorB, 'r', t_sec, motorC, 'g');
ylabel('Motor Power');
title('Motor Commands');
legend('A (left)', 'B (right)', 'C (fwd)', 'Location', 'best');
grid on;
% Shade forward green, reverse red
hold on;
yl = ylim;
if ~isempty(fwd_start) && ~isempty(fwd_end)
    patch([t_sec(fwd_start) t_sec(fwd_end) t_sec(fwd_end) t_sec(fwd_start)], ...
          [yl(1) yl(1) yl(2) yl(2)], 'g', 'FaceAlpha', 0.08, 'EdgeColor', 'none');
end
if ~isempty(rev_start) && ~isempty(rev_end)
    patch([t_sec(rev_start) t_sec(rev_end) t_sec(rev_end) t_sec(rev_start)], ...
          [yl(1) yl(1) yl(2) yl(2)], 'r', 'FaceAlpha', 0.08, 'EdgeColor', 'none');
end
hold off;

subplot(3,1,2);
plot(t_sec, headingIMU, 'k', 'LineWidth', 1);
ylabel('Heading (deg)');
title('IMU Heading');
grid on;

subplot(3,1,3);
plot(t_sec, rollIMU, 'b', t_sec, pitchIMU, 'r');
ylabel('Angle (deg)');
xlabel('Time (s)');
title('Roll & Pitch');
legend('Roll', 'Pitch');
grid on;

%% ---- FIGURE 2: WIND DATA ----
figure(2); clf;
sgtitle('Phake Lake Test - Wind Sensors');

subplot(3,1,1);
plot(t_sec, anemo_freq, 'b');
ylabel('Frequency (Hz)');
title('Anemometer Rotation');
grid on;

subplot(3,1,2);
plot(t_sec, wind_speed, 'r', 'LineWidth', 1);
ylabel('Wind Speed (m/s)');
title('Wind Speed (from calibration)');
grid on;

subplot(3,1,3);
plot(t_sec, vane_angle, 'c', 'LineWidth', 1);
ylabel('Angle (deg)');
xlabel('Time (s)');
title('Weather Vane Direction');
grid on;

%% ---- FIGURE 3: GPS TRACK ----
has_gps = (nsats >= 5);
figure(3); clf;

if any(has_gps)
    subplot(1,2,1);
    plot(double(lon(has_gps)), double(lat(has_gps)), 'b.-', 'MarkerSize', 8);
    hold on;
    % Mark start
    first_gps = find(has_gps, 1);
    plot(double(lon(first_gps)), double(lat(first_gps)), 'go', 'MarkerSize', 12, 'LineWidth', 2);
    % Mark end
    last_gps = find(has_gps, 1, 'last');
    plot(double(lon(last_gps)), double(lat(last_gps)), 'rs', 'MarkerSize', 12, 'LineWidth', 2);
    % Mark forward vs reverse
    fwd_gps = has_gps & is_forward;
    rev_gps = has_gps & is_reverse;
    if any(rev_gps)
        plot(double(lon(rev_gps)), double(lat(rev_gps)), 'r.', 'MarkerSize', 8);
    end
    xlabel('Longitude');
    ylabel('Latitude');
    title('GPS Track');
    legend('Track', 'Start', 'End', 'Reverse', 'Location', 'best');
    grid on;
    axis equal;
    hold off;

    subplot(1,2,2);
    plot(t_sec, double(nsats), 'k.-');
    ylabel('Satellites');
    xlabel('Time (s)');
    title('GPS Satellites');
    grid on;
    ylim([0 max(double(nsats))+2]);
else
    text(0.5, 0.5, 'No GPS fix acquired (nsats < 5)', ...
         'HorizontalAlignment', 'center', 'FontSize', 14);
    title('GPS Track - No Fix');
end

%% ---- FIGURE 4: MAGNETOMETER ----
figure(4); clf;
sgtitle('Magnetometer Data');

subplot(1,2,1);
scatter(magX, magY, 4, 'b', 'filled');
xlabel('magX (uTesla)');
ylabel('magY (uTesla)');
title('magX vs magY');
axis equal; grid on;

subplot(1,2,2);
plot(t_sec, magX, 'r', t_sec, magY, 'g', t_sec, magZ, 'b');
xlabel('Time (s)');
ylabel('Magnetic Field (uTesla)');
title('Magnetometer vs Time');
legend('X', 'Y', 'Z');
grid on;

%% ---- SUMMARY STATS ----
fprintf('\n=== Summary ===\n');
fprintf('%-20s %10s %10s\n', '', 'Forward', 'Reverse');
fprintf('%s\n', repmat('-', 1, 42));

if ~isempty(fwd_start) && ~isempty(fwd_end)
    fwd_idx = fwd_start:fwd_end;
else
    fwd_idx = [];
end
if ~isempty(rev_start) && ~isempty(rev_end)
    rev_idx = rev_start:rev_end;
else
    rev_idx = [];
end

if ~isempty(fwd_idx)
    fprintf('%-20s %10.1f', 'Avg heading (deg)', mean(headingIMU(fwd_idx)));
else
    fprintf('%-20s %10s', 'Avg heading (deg)', 'N/A');
end
if ~isempty(rev_idx)
    fprintf(' %10.1f\n', mean(headingIMU(rev_idx)));
else
    fprintf(' %10s\n', 'N/A');
end

if ~isempty(fwd_idx)
    fprintf('%-20s %10.2f', 'Avg wind (m/s)', mean(wind_speed(fwd_idx)));
else
    fprintf('%-20s %10s', 'Avg wind (m/s)', 'N/A');
end
if ~isempty(rev_idx)
    fprintf(' %10.2f\n', mean(wind_speed(rev_idx)));
else
    fprintf(' %10s\n', 'N/A');
end

if ~isempty(fwd_idx)
    fprintf('%-20s %10.1f', 'Avg vane (deg)', mean(double(vane_angle(fwd_idx))));
else
    fprintf('%-20s %10s', 'Avg vane (deg)', 'N/A');
end
if ~isempty(rev_idx)
    fprintf(' %10.1f\n', mean(double(vane_angle(rev_idx))));
else
    fprintf(' %10s\n', 'N/A');
end

if any(has_gps)
    gps_fwd = has_gps & is_forward;
    gps_rev = has_gps & is_reverse;
    if any(gps_fwd)
        fprintf('%-20s %10.6f', 'Avg lat', mean(double(lat(gps_fwd))));
    else
        fprintf('%-20s %10s', 'Avg lat', 'N/A');
    end
    if any(gps_rev)
        fprintf(' %10.6f\n', mean(double(lat(gps_rev))));
    else
        fprintf(' %10s\n', 'N/A');
    end
end

fprintf('\nTotal duration: %.1f seconds\n', t_sec(end));

%% ---- EXPORT CSV ----
csvfile = fullfile(filepath, ['phakelake_' filenum '.csv']);

% Build data table with all columns
T = table( ...
    t_sec, ...
    double(rollIMU), ...
    double(pitchIMU), ...
    double(headingIMU), ...
    double(accelX), ...
    double(accelY), ...
    double(accelZ), ...
    double(magX), ...
    double(magY), ...
    double(magZ), ...
    double(lat), ...
    double(lon), ...
    double(nsats), ...
    double(motorA), ...
    double(motorB), ...
    double(motorC), ...
    double(anemo_freq), ...
    double(wind_speed), ...
    double(vane_angle), ...
    double(vane_voltage) ...
);

% Column headers with units
T.Properties.VariableNames = { ...
    'time_s', ...
    'roll_deg', ...
    'pitch_deg', ...
    'heading_deg', ...
    'accelX_mg', ...
    'accelY_mg', ...
    'accelZ_mg', ...
    'magX_uTesla', ...
    'magY_uTesla', ...
    'magZ_uTesla', ...
    'lat_deg', ...
    'lon_deg', ...
    'nsats', ...
    'motorA_pwm', ...
    'motorB_pwm', ...
    'motorC_pwm', ...
    'anemo_freq_Hz', ...
    'wind_speed_mps', ...
    'vane_angle_deg', ...
    'vane_voltage_V' ...
};

writetable(T, csvfile);
fprintf('\nCSV exported to: %s\n', csvfile);
