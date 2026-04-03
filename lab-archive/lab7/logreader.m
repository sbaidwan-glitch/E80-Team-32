% logreader.m
% Use this script to read data from your micro SD card

clear;
%clf;

filenum = '008'; % file number for the data you want to read
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
items = textscan(fileID,'%s','Delimiter',',','EndOfLine','\r\n');
fclose(fileID);
[ncols,~] = size(items{1});
ncols = ncols/2;
varNames = items{1}(1:ncols)';
varTypes = items{1}(ncols+1:end)';
varLengths = zeros(size(varTypes));
colLength = 256;
for i = 1:numel(varTypes)
    varLengths(i) = dataSizes.(varTypes{i});
end
R = cell(1,numel(varNames));

%% read column-by-column from datafile
fid = fopen(datafile,'rb');
for i=1:numel(varTypes)
    %# seek to the first field of the first record
    fseek(fid, sum(varLengths(1:i-1)), 'bof');
    
    %# % read column with specified format, skipping required number of bytes
    R{i} = fread(fid, Inf, ['*' varTypes{i}], colLength-varLengths(i));
    eval(strcat(varNames{i},'=','R{',num2str(i),'};'));
end
fclose(fid);

%% Process your data here

dt = 0.099;
N = length(accelX);
t = ((0:N-1) * dt)';

ax = double(accelX) * 0.0102;     % Convert from teensy to m/s^2
ay = double(accelY) * 0.0102;

n_rest = round(2/dt);
ax = ax - mean(ax(1:n_rest));
ay = ay - mean(ay(1:n_rest));

sigma = std(ay(1:n_rest));         % The standard deviation of the noise in the accel
confLev = 0.95;                    % The confidence level for bounds
preie = sqrt(2)*erfinv(confLev)*sigma*sqrt(dt);  % the prefix to the sqrt(t)
preiie = 2/3*preie;                % The prefix to t^3/2
plusiie = preiie*t.^1.5;          % The positive noise bound for double integration

vx = cumtrapz(t, ax);
vy = cumtrapz(t, ay);
pos_x = cumtrapz(t, vx);
pos_y = cumtrapz(t, vy);

rnp = pos_y + plusiie;            % Position plus confidence bound
rnm = pos_y - plusiie;            % Position minus confidence bound

figure(1)
plot(pos_x, pos_y)
hold on
plot([0 0.5 0], [0 0 0], 'r--', 'LineWidth', 2)
hold off
xlabel('X position (m)')
ylabel('Y position (m)')
title('Integrated IMU Path (X-Y Plane)')
legend('Integrated path', 'Ideal 0.5 m path', 'location', 'best')
grid on

figure(2)
plot(t, pos_y, t, rnp, '-.', t, rnm, '-.')
xlabel('Time (s)')
ylabel('Y position (m)')
title('Y Position vs Time')
legend('Y position', 'Upper Confidence Bound', ...
    'Lower Confidence Bound', 'location', 'best')
grid on