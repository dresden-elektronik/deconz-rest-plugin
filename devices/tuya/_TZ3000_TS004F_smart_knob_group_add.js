// Generate contents for a Group Add command (value 0x00) to a Tuya Smart Knob on the GROUP cluster (ID 0x0004).
// Uses the first defined group (group 0) only, as the device only supports one group at a time.
// Does not check for existing membership, we're unable to send a Group View command from this script.
// global Item (string of comma-separated group ID numbers), R, ZclFrame

let group = Number(Item.val.split(',')[0]);

if (isNaN(group)) {
    // Group might be "auto", which this script cannot resolve. Don't try to add a group.
    throw new TypeError('Invalid group ID: '  + Item.val);
} else {
    const group_hexstr = (group & 0xFF).toString(16) + (group >> 8).toString(16).padStart(2, '0');
    const name_hexstr = '00'; // Empty string for the group name
    group_hexstr + name_hexstr;
}
