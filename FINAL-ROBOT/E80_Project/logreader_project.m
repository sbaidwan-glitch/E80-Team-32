% logreader_project.m

% 2. "How do I know what columns exist?"
%    -> Open the INF*.TXT file from your SD card in a text editor. It's a
%       CSV listing all variable names and their types. The names match
%       the first argument of each DataSource constructor (e.g.,
%       "als_raw,lux" in SensorVEML6030.cpp). The logreader's eval()
%       call creates MATLAB variables with these exact names.
%
% 3. "How do I convert ADC values to voltage?"
%    -> Teensy 3.2 default: 10-bit ADC (0-1023), 3.3V reference.
%       voltage = raw * 3.3 / 1023. This is in the Teensy documentation
%       (pjrc.com/teensy/adc.html) and the Arduino analogRead() reference.
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
%              (A01 = BPW34 TIA output on pin 15)
%              (A02 = pin 16, noise -- ignore, used for anemometer digital)
%   Errors:    ErrorFlagA, ErrorFlagB, ErrorFlagC
%   Button:    Button
%   VEML6030:  als_raw, lux
%   Anemometer: anemo_freq, wind_speed
%   WeatherVane: vane_angle, vane_voltage

clear;

filenum = '001';  % file number for the data you want to read
infofile = strcat('INF', filenum, '.TXT');
datafile = strcat('LOG', filenum, '.BIN');

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

%% Convert BPW34 ADC reading to voltage
% ADC is 10-bit (0-1023), reference is 3.3V
% A01 column holds the BPW34 transimpedance amplifier output
bpw34_voltage = double(A01) * 3.3 / 1023;

%% Identify station periods (motors off = station stop)
% motorA and motorB are both 0 when stopped
is_stopped = (motorA == 0) & (motorB == 0);

%% ---- PLOTS ----

figure(1); clf;
sgtitle('Team 32 - Sensor Data vs Time');

% Wind speed
subplot(5, 1, 1);
plot(t_min, wind_speed, 'b');
ylabel('Wind Speed (m/s)');
title('Anemometer');
grid on;
% Shade station periods
hold on;
yl = ylim;
for i = 1:length(is_stopped)
    if is_stopped(i)
        patch([t_min(i)-dt/120, t_min(i)+dt/120, t_min(i)+dt/120, t_min(i)-dt/120], ...
              [yl(1), yl(1), yl(2), yl(2)], 'g', 'FaceAlpha', 0.1, 'EdgeColor', 'none');
    end
end
hold off;

% Ambient light (VEML6030)
subplot(5, 1, 2);
plot(t_min, lux, 'r');
ylabel('Lux');
title('VEML6030 Ambient Light');
grid on;

% BPW34 photodiode voltage
subplot(5, 1, 3);
plot(t_min, bpw34_voltage, 'm');
ylabel('Voltage (V)');
title('BPW34 Photodiode (TIA output on A01)');
grid on;

% Wind direction (weather vane)
subplot(5, 1, 4);
plot(t_min, vane_angle, 'c');
ylabel('Angle (deg)');
title('Weather Vane Direction');
grid on;

% Heading
subplot(5, 1, 5);
plot(t_min, headingIMU, 'k');
ylabel('Heading (deg)');
xlabel('Time (min)');
title('IMU Heading');
grid on;

%% GPS track (if satellites were acquired)
if exist('lat', 'var') && exist('lon', 'var')
    has_gps = (nsats >= 5);
    if any(has_gps)
        figure(2); clf;
        plot(double(lon(has_gps)), double(lat(has_gps)), 'b.-');
        xlabel('Longitude');
        ylabel('Latitude');
        title('GPS Track');
        grid on;
        % Mark station stops on map
        hold on;
        stopped_gps = has_gps & is_stopped;
        if any(stopped_gps)
            plot(double(lon(stopped_gps)), double(lat(stopped_gps)), 'ro', 'MarkerSize', 4);
        end
        legend('Track', 'Station stops');
        hold off;
    end
end

%% Station-averaged data summary
fprintf('\n=== Station Averages ===\n');
fprintf('%-10s %-12s %-12s %-15s %-14s %-12s\n', ...
        'Station', 'Wind (m/s)', 'Lux', 'BPW34 (V)', 'Vane (deg)', 'Heading');
fprintf('%s\n', repmat('-', 1, 80));

% Find station boundaries (transitions from driving to stopped)
stop_starts = find(diff([0; is_stopped]) == 1);
stop_ends   = find(diff([is_stopped; 0]) == -1);
n_stations = min(length(stop_starts), length(stop_ends));

for s = 1:n_stations
    idx = stop_starts(s):stop_ends(s);
    fprintf('%-10d %-12.2f %-12.1f %-15.3f %-14.1f %-12.1f\n', ...
            s, ...
            mean(wind_speed(idx)), ...
            mean(lux(idx)), ...
            mean(bpw34_voltage(idx)), ...
            mean(double(vane_angle(idx))), ...
            mean(headingIMU(idx)));
end

fprintf('\nTotal samples: %d (%.1f minutes)\n', N, t_min(end));
