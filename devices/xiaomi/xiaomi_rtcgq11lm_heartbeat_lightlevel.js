// Tag 0x0B of the 0xFF01 heartbeat holds the illuminance in lux, as cluster 0x0400 does on this sensor.
// Match the legacy Xiaomi handler: tag 0x0B is the light level only as uint16.
if (Attr.dataType === 0x21) {
    const tholddark = R.item('config/tholddark').val;
    const tholdoffset = R.item('config/tholdoffset').val;
    const lux = Attr.val;
    var ll = 0;

    if (lux > 0 && lux < 0xffff) {
        ll = Math.round(10000 * Math.log10(lux) + 1);
    }

    R.item('state/lux').val = lux;
    R.item('state/dark').val = ll <= tholddark;
    R.item('state/daylight').val = ll >= tholddark + tholdoffset;

    Item.val = ll;
}
