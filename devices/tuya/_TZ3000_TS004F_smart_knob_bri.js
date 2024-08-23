// Handle a STEP command (value 0x02) from a Tuya Smart Knob to the LEVEL_CONTROL cluster (ID 0x0008).
// Translate into a button event with accompanying rotation angle and eventduration
// global Item (here referring to the state/bri is unused), R, ZclFrame

let btnev;

// Rotate in command mode without holding button down
// Translate a Zigbee LEVEL_CONTROL STEP command to a rotation
// Frame is enum8 step mode, uint8 step size, uint16 transition time (tenths of a second)
const step_mode = ZclFrame.at(0);
const step_size = ZclFrame.at(1);
const transition_time = ZclFrame.at(3) << 8 | ZclFrame.at(2);
// Multiplication factor scaling uint8 to 360 degrees, with actual range for step being 1 to 240, gives:
// angle = (step_size - step_min) * 360 / (step_max - step_min + 1);
let angle = (step_size - 1) * 1.5;
if (step_mode === 0) { // Up
    btnev = 1030; // Button 1, rotate clockwise
} else if (step_mode === 1) { // Down
    btnev = 1031; // Button 1, rotate counter-clockwise
    angle = -angle;
}

if (btnev !== undefined) {
    R.item('state/angle').val = angle;
    R.item('state/eventduration').val = transition_time;
    R.item('state/buttonevent').val = btnev;
}