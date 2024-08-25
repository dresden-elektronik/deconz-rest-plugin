// Handle a command from a Tuya Smart Knob to the ONOFF cluster (ID 0x0006).
// Translate into a button event with accompanying rotation angle and eventduration
// global Item (here referring to the state/on is unused), R, ZclFrame

const cmd = ZclFrame.cmd;
let btnev;
let angle = 0; // relative angle in degrees, default to no rotation
let eventduration = 0; // time in 1/10 seconds, not sent for ONOFF commands so default to 0

if (cmd === 0x01) { // ON
    // Sent by knob when twisting after a period of inactivity. Probably to turn on linked lights before adjusting them.
    // Ignored here, we're only interested in the following STEP command.
} else if (cmd === 0x02) { // TOGGLE
    // Single click in command mode
    btnev = 1002; // Button 1, short release
} else if (cmd === 0xFC) { // ???
    const pl0 = ZclFrame.at(0);
    // Rotate in event mode without holding down
    if (pl0 === 0) {
        btnev = 3030; // Button 3, rotate clockwise
        // No angle or time sent by button in event mode, and doesn't send an event for every notch that it's rotated.
        // So don't report an angle or time.
    } else if (pl0 === 1) {
        btnev = 3031; // Button 3, rotate counter-clockwise
        // No angle or time sent, as above
    }
} else if (cmd === 0xFD) { // LIDL
    const pl0 = ZclFrame.at(0);
    // Button presses while in event mode
    if (pl0 === 0) {
        btnev = 3002; // Button 3, short release
    } else if (pl0 === 1) {
        btnev = 3004; // Button 3, double press
    } else if (pl0 === 2) {
        btnev = 3003; // Button 3, long release
    }
}

if (btnev !== undefined) {
    R.item('state/angle').val = angle;
    R.item('state/eventduration').val = eventduration;
    R.item('state/buttonevent').val = btnev;
}