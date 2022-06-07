let mode = Attr.val;

if (mode === 0) mode = 'off';
else if (mode === 1) mode = 'auto';
else if (mode <= 10) mode = 'speed_1';
else if (mode <= 20) mode = 'speed_2';
else if (mode <= 30) mode = 'speed_3';
else if (mode <= 40) mode = 'speed_4';
else mode = 'speed_5';

Item.val = mode;