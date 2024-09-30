// Handle a Group View Membership Response command (value 0x02) from a Tuya Smart Knob to the GROUP cluster (ID 0x0004).
// Extracts the last group only. The device only sends groupcasts to the last added group.
// global Item (string of comma-separated group ID numbers), R, ZclFrame

// const capacity = ZclFrame.at(0); // unused
const group_count = ZclFrame.at(1);

if (group_count > 0) {
    // Groups IDs are a list of uint16s starting at offset 2
    const last_group = ZclFrame.at(group_count * 2 + 1) << 8 | ZclFrame.at(group_count * 2);
    Item.val = last_group;
} else {
    // No groups set
    // deCONZ's JS engine won't accept an empty string, so use a single space (which gets converted to an empty string)
    Item.val = " ";
}
