% patch_synthetic_cloudy.m
%
% Same destructive in-place patcher as patch_synthetic.m, but generates
% values characteristic of a partly-cloudy / overcast day instead of
% clear sunshine. Lower mean irradiance and larger variability to capture
% passing clouds blocking/unblocking the sun.
%
% A .bak copy of each file is written before overwriting, so you can
% restore by renaming the .bak files if something goes wrong.
%
% Usage: set filenum (and dataDir if the LOG/INF aren't on the Desktop),
%        then run. No arguments required.
%
% Generated columns:
%   irradiance_W_m2  uniform [300, 600] W/m^2      (cloudy range)
%   veml_lux         100 * irradiance + N(0, 1000) (more cloud flicker)
%   bpw_voltage      0.003 * irradiance + 0.05 + N(0, 0.03)
%
% Typical output ranges:
%   lux       ~ 30,000 - 60,000  with ~1000 lux sigma
%   voltage   ~ 0.95 - 1.85 V    with ~30 mV sigma
%
% Reference values (for intuition):
%   Overcast:        100 - 300 W/m^2     ~ 10k - 30k lux
%   Partly cloudy:   300 - 700 W/m^2     ~ 30k - 70k lux
%   Clear sunny:     700 - 950 W/m^2     ~ 70k - 95k lux

clear;

%% -------- User settings --------
filenum = '035';                                   % LOG###.BIN / INF###.TXT number
dataDir = fullfile(getenv('HOME'), 'Desktop');     % where LOG/INF live

%% -------- Synthetic generation parameters (cloudy-ish day) --------
IRR_MIN     = 300;    IRR_MAX     = 600;    % W/m^2, uniform
LUX_PER_IRR = 100;    LUX_SIGMA   = 1000;   % Gaussian noise (wider -- cloud flicker)
BPW_SLOPE   = 0.003;  BPW_OFFSET  = 0.05;   % V per W/m^2, bias
BPW_SIGMA   = 0.03;                         % Gaussian noise on voltage

NEW_NAMES = {'irradiance_W_m2', 'veml_lux', 'bpw_voltage'};
NEW_TYPES = {'float', 'float', 'float'};    % single-precision, 4 bytes each

BLOCK_SIZE = 256;

%% -------- Paths --------
infPath = fullfile(dataDir, sprintf('INF%s.TXT', filenum));
binPath = fullfile(dataDir, sprintf('LOG%s.BIN', filenum));

if ~isfile(infPath), error('Missing INF file: %s', infPath); end
if ~isfile(binPath), error('Missing LOG file: %s', binPath); end

%% -------- Backups (safety net before destructive write) --------
copyfile(infPath, [infPath '.bak']);
copyfile(binPath, [binPath '.bak']);
fprintf('Backups written:\n  %s.bak\n  %s.bak\n', infPath, binPath);

%% -------- Parse existing INF schema --------
fileID = fopen(infPath, 'r');
items = textscan(fileID, '%s', 'Delimiter', ',', 'EndOfLine', '\r\n');
fclose(fileID);
nTokens = numel(items{1});
if mod(nTokens, 2) ~= 0
    error('INF file has odd number of tokens (%d) -- corrupt schema?', nTokens);
end
nCols    = nTokens / 2;
varNames = items{1}(1:nCols);
varTypes = items{1}(nCols+1:end);

sizesMap = containers.Map( ...
    {'float','ulong','int','int32','uint8','uint16','char','bool'}, ...
    { 4,      4,      4,    4,      1,      2,       1,     1});
varLens   = cellfun(@(t) sizesMap(t), varTypes);
usedBytes = sum(varLens);
newBytes  = 4 * numel(NEW_NAMES);

fprintf('Existing columns: %d  (used %d of %d bytes per block)\n', ...
        nCols, usedBytes, BLOCK_SIZE);

if usedBytes + newBytes > BLOCK_SIZE
    error(['No room in 256-byte block: used=%d, adding=%d, block=%d.\n' ...
           'Drop one or more synthetic columns, or shrink an existing one.'], ...
           usedBytes, newBytes, BLOCK_SIZE);
end

%% -------- Read the entire binary log as raw bytes --------
fid = fopen(binPath, 'rb');
bytes = fread(fid, Inf, '*uint8');
fclose(fid);

fileSize = numel(bytes);
if mod(fileSize, BLOCK_SIZE) ~= 0
    error('Log file size %d is not a multiple of %d.', fileSize, BLOCK_SIZE);
end
N = fileSize / BLOCK_SIZE;
fprintf('Total samples in log: %d (%.1f min @ 10.1 Hz)\n', N, N * 0.099 / 60);

%% -------- Generate synthetic data (fresh Gaussian noise each run) --------
rng('shuffle');
irr = IRR_MIN + (IRR_MAX - IRR_MIN) * rand(N, 1);
lux = LUX_PER_IRR * irr + LUX_SIGMA * randn(N, 1);
bpw = BPW_SLOPE   * irr + BPW_OFFSET + BPW_SIGMA * randn(N, 1);

% Clamp physically-impossible negatives (can happen when mean is low and sigma is wide)
lux = max(lux, 0);
bpw = max(bpw, 0);

%% -------- Convert to little-endian float bytes --------
irrBytes = reshape(typecast(single(irr), 'uint8'), 4, N)';
luxBytes = reshape(typecast(single(lux), 'uint8'), 4, N)';
bpwBytes = reshape(typecast(single(bpw), 'uint8'), 4, N)';

synthBytes = [irrBytes, luxBytes, bpwBytes];

%% -------- Splice synthetic bytes into each 256-byte block --------
blocks = reshape(bytes, BLOCK_SIZE, N)';
blocks(:, usedBytes + 1 : usedBytes + newBytes) = synthBytes;

newRawBytes = reshape(blocks', [], 1);

%% -------- Write updated BIN (in place) --------
fid = fopen(binPath, 'wb');
fwrite(fid, newRawBytes, 'uint8');
fclose(fid);

%% -------- Write updated INF (in place) with new columns appended --------
allNames = [varNames(:); NEW_NAMES(:)];
allTypes = [varTypes(:); NEW_TYPES(:)];

fid = fopen(infPath, 'w');
fprintf(fid, '%s', strjoin(allNames, ','));
fprintf(fid, '\r\n');
fprintf(fid, '%s', strjoin(allTypes, ','));
fprintf(fid, '\r\n');
fclose(fid);

fprintf('\nPatch complete (cloudy-day synthetic data).\n');
fprintf('  %s   (added %d columns)\n', binPath, numel(NEW_NAMES));
fprintf('  %s   (schema updated)\n', infPath);
fprintf('Restore: mv %s.bak %s  &&  mv %s.bak %s\n', ...
        infPath, infPath, binPath, binPath);
