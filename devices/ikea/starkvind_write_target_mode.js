let mode = Item.val;

if (mode === 'off') mode = 0;
else if (mode === 'auto') mode = 1;
else if (mode === 'speed_1') mode = 10;
else if (mode === 'speed_2') mode = 20;
else if (mode === 'speed_3') mode = 30;
else if (mode === 'speed_4') mode = 40;
else if (mode === 'speed_5') mode = 50;
else mode = 0;

mode;