// Handle a command from a Tuya Smart Knob to the COLOR_CONTROL cluster (ID 0x0300).
// Translate into a button event with accompanying rotation angle and eventduration
// global Item (here referring to the state/ct is unused), R, ZclFrame

const cmd = ZclFrame.cmd;
let btnev;
let angle = 0; // relative angle in degrees, default to no rotation
let eventduration = 0; // time in 1/10 seconds

if (cmd === 0x01) { // MOVE_HUE
    // Sent one second after MOVE_SATURATION when the knob is held down in command mode
    // Ignore it here, it doesn't provide any extra information
} else if (cmd === 0x04) { // MOVE_SATURATION
    btnev = 1001; // Button 1, hold
} else if (cmd === 0x47) { // STOP_MOVE_STEP
    btnev = 1003; // Button 1, long release
} else if (cmd === 0x4C) { // STEP_COLOR_TEMPERATURE
    // Rotate in command mode while holding down
    // Translate a Zigbee COLOR_CONTROL STEP_COLOR_TEMPERATURE command to a rotation
    // Frame is enum8 step mode, uint16 step size, uint16 transition time (tenths of a second), uint16 color temperature minimum (mireds), uint16 color temperature maximum (mireds)
    const step_mode = ZclFrame.at(0);
    const step_size = ZclFrame.at(2) << 8 | ZclFrame.at(1);
    const transition_time = ZclFrame.at(4) << 8 | ZclFrame.at(3);
    // This time the range is 1 to 340
    angle = (step_size - 1) * 360 / 340;
    eventduration = transition_time;
    if (step_mode === 1) { // Step Hue Up
        btnev = 2030; // Button 2, rotate clockwise
    } else if (step_mode === 3) { // Step Hue Down
        btnev = 2031; // Button 2, rotate counter-clockwise
        angle = -angle;
    }
}

if (btnev !== undefined) {
    R.item('state/angle').val = angle;
    R.item('state/eventduration').val = eventduration;
    R.item('state/buttonevent').val = btnev;
}