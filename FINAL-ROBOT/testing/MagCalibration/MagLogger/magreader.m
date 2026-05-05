% magreader.m
%
% Reads a MAG_XXXX.CSV produced by MagLogger.ino and plots the raw vs
% calibrated magnetometer circle plus the heading ramp.
%
% Usage:
%   1. Drop MAG_XXXX.CSV somewhere (default: ~/Desktop).
%   2. Set filenum below to the four-digit log number.
%   3. Run.
%
% CSV columns (from MagLogger.ino line 107):
%   Time_ms, magX_raw, magY_raw, magZ_raw,
%   magX_cal, magY_cal, magZ_cal, heading, roll, pitch
%
% Units: mag in uTesla, heading/roll/pitch in degrees.

clear;

%% -------- User settings --------
filenum = '0003';      % MAG_####.CSV number
dataDir = fullfile(getenv('HOME'), 'Desktop');

%% -------- Load CSV --------
csvPath = fullfile(dataDir, ['MAG_' filenum '.CSV']);
if ~isfile(csvPath)
    error('magreader:fileNotFound', 'Could not find %s', csvPath);
end

M = readmatrix(csvPath);
if isempty(M)
    error('magreader:emptyFile', 'File %s has no numeric rows', csvPath);
end

t_s     = M(:, 1) / 1000;
mx_raw  = M(:, 2);   my_raw  = M(:, 3);   mz_raw  = M(:, 4);
mx_cal  = M(:, 5);   my_cal  = M(:, 6);   mz_cal  = M(:, 7);
heading = M(:, 8);
roll    = M(:, 9);
pitch   = M(:,10);

N = size(M, 1);
fprintf('Loaded %d samples (%.1f s) from %s\n', N, t_s(end), csvPath);

%% -------- Basic stats --------
fprintf('\n--- Raw magnetometer ---\n');
fprintf('  magX: mean %+7.2f uT, range [%+7.2f, %+7.2f]\n', mean(mx_raw), min(mx_raw), max(mx_raw));
fprintf('  magY: mean %+7.2f uT, range [%+7.2f, %+7.2f]\n', mean(my_raw), min(my_raw), max(my_raw));
fprintf('  magZ: mean %+7.2f uT, range [%+7.2f, %+7.2f]\n', mean(mz_raw), min(mz_raw), max(mz_raw));

% Hard-iron estimate from raw extrema (quick-and-dirty — real calibration
% should use Mag_Calibration.m, but this gives a ballpark offset).
offset_x_est = (max(mx_raw) + min(mx_raw)) / 2;
offset_y_est = (max(my_raw) + min(my_raw)) / 2;
offset_z_est = (max(mz_raw) + min(mz_raw)) / 2;
fprintf('\nQuick hard-iron offset estimate (min/max midpoint):\n');
fprintf('  [ %+7.3f, %+7.3f, %+7.3f ]  uT\n', ...
        offset_x_est, offset_y_est, offset_z_est);

%% -------- Plot 1: mx vs my (horizontal circle) --------
figure(1); clf;
scatter(mx_raw, my_raw, 12, 'r', 'filled'); hold on;
scatter(mx_cal, my_cal, 12, 'b', 'filled');
plot(0, 0, 'k+', 'MarkerSize', 12, 'LineWidth', 1.5);
axis equal; grid on;
xlabel('magX [\muT]');
ylabel('magY [\muT]');
legend('Raw', 'Calibrated', 'Origin', 'Location', 'best');
title(['MAG\_' filenum ' - Horizontal circle (should be centered at origin)']);

%% -------- Plot 2: heading vs time --------
figure(2); clf;
plot(t_s, heading, 'k.-'); grid on;
xlabel('Time [s]');
ylabel('Heading [deg]');
ylim([-200 200]);
title(['MAG\_' filenum ' - Heading during spin (should ramp smoothly)']);

%% -------- Plot 3: 3D scatter (sphere check) --------
figure(3); clf;
scatter3(mx_raw, my_raw, mz_raw, 10, 'r', 'filled'); hold on;
scatter3(mx_cal, my_cal, mz_cal, 10, 'b', 'filled');
plot3(0, 0, 0, 'k+', 'MarkerSize', 12, 'LineWidth', 1.5);
axis equal; grid on;
xlabel('magX [\muT]'); ylabel('magY [\muT]'); zlabel('magZ [\muT]');
legend('Raw', 'Calibrated', 'Origin', 'Location', 'best');
title(['MAG\_' filenum ' - 3D sphere (tilt + spin coverage)']);
view(35, 25);

%% -------- Plot 4: raw components vs time --------
figure(4); clf;
subplot(3, 1, 1);
plot(t_s, mx_raw, 'r'); grid on;
ylabel('magX [\muT]'); title('Raw magnetometer components vs time');

subplot(3, 1, 2);
plot(t_s, my_raw, 'g'); grid on;
ylabel('magY [\muT]');

subplot(3, 1, 3);
plot(t_s, mz_raw, 'b'); grid on;
ylabel('magZ [\muT]'); xlabel('Time [s]');

fprintf('\nDone. Inspect Figure 1 -- a clean circle centered at origin means good calibration.\n');
